/*Copyright (C) 2017 kXuan <kxuanobj@gmail.com>

   This file is part of the lrpc.
   The lrpc is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.
   The lrpc is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   You should have received a copy of the GNU Lesser General Public
   License along with the lrpc; if not, see
   <http://www.gnu.org/licenses/>.*/

#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <lrpc-internal.h>
#include "config.h"
#include "interface.h"
#include "msg.h"

void endpoint_init(struct lrpc_endpoint *endpoint, struct lrpc_interface *inf, const char *name, size_t name_len)
{
	char *p;

	assert(endpoint && name_len && name);

	endpoint->inf = inf;
	endpoint->addr.sun_family = AF_UNIX;
	p = endpoint->addr.sun_path;
	*p++ = 0;
	memcpy(p, LRPC_MSG_NAME_PREFIX, sizeof(LRPC_MSG_NAME_PREFIX) - 1);
	p += sizeof(LRPC_MSG_NAME_PREFIX) - 1;
	memcpy(p, name, name_len);
	p += name_len;
	endpoint->addr_len = (socklen_t) (offsetof(struct sockaddr_un, sun_path) + (p - endpoint->addr.sun_path));
}

struct sync_call_context
{
	int done;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	void *ret_ptr;
	size_t ret_size;
	int err_code;
};

static void sync_call_finish(struct lrpc_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
{
	struct sync_call_context *sync_ctx = ctx->user_data;
	size_t copy_size;
	pthread_mutex_lock(&sync_ctx->lock);
	if (err_code != 0) {
		sync_ctx->err_code = err_code;
	} else {
		copy_size = ret_size > sync_ctx->ret_size ? sync_ctx->ret_size : ret_size;
		memcpy(sync_ctx->ret_ptr, ret_ptr, copy_size);
		sync_ctx->ret_size = copy_size;
	}
	sync_ctx->done = 1;
	pthread_cond_broadcast(&sync_ctx->cond);
	pthread_mutex_unlock(&sync_ctx->lock);
}

static int
endpoint_call_and_wait(struct lrpc_endpoint *endpoint,
                       struct lrpc_call_ctx *async_ctx, struct sync_call_context *sync_ctx,
                       const char *method_name, const void *args, size_t args_len)
{
	int rc, poll_rc = 0;
	struct lrpc_interface *inf = endpoint->inf;

	rc = pthread_mutex_trylock(&inf->lock_poll);
	if (rc == 0) {
		/* ok .. we are the one it is so good.
		 * FIXME the callback of async call is unexpected called by us.
		 *       The callback of async call should be called by the thread which is calling lrpc_poll().
         *
		 * There are some solutions:
		 *  1) save all request
		 *      * The first thread allocate a new buffer to receive packet, the other thread wait.
		 *      * when the first thread receive a new packet, it wake up the correct thread and send the packet to it.
		 *      * Then the first thread create a new buffer to receive another packet.
		 *      * When A has received the packet it wants, it wake up another thread, and handle its packet.
		 *
		 *      We must dynamic allocate memory to receive packet or we must copy packet, which is a little slow.
		 *      This solution improves performance by consuming more memory.
		 *
		 *  2) only one async request
		 *      * refuse more than one thread to call lrpc_poll()
		 *      * when a packet has been received on a thread with sync context,
		 *        > check if the packet is matched with itself, if it is matched, finish the sync context.
		 *        > check if the packet is matched with another thread with sync context, copy the returns data to the
		 *          application, and wake up the matched thread to finish the sync context.
		 *        > wake up the thread which is calling lrpc_poll(), and go to wait its sync context.
		 *      * when a packet has been received on the thread calling lrpc_poll()
		 *        > check if the packet is matched with a thread with sync context, copy the returns data to the
		 *          application, and wake up the matched thread to finish the sync context.
		 *        > handle the packet normally in async context.
		 *
		 *      There is no need to allocate memory in this solution, but the thread which is chosen to receive messages
		 *      must copy packet data (returns data) with only 1 cpu. It is slower than the first solution in multi-CPU
		 *      platform.
		 *
		 *  3) currently
		 *     we call the callback with this thread, so it is a bug.
		 */
		rc = lrpc_call_async(endpoint, async_ctx, method_name, args, args_len, sync_call_finish);
		if (rc == 0) {
			while (sync_ctx->done == 0 && poll_rc == 0) {
				poll_rc = inf_poll_unsafe(endpoint->inf, &endpoint->inf->packet_recv,
				                          sizeof(endpoint->inf->packet_recv.payload));
			}
			if (sync_ctx->done == 0) {
				rc = poll_rc;
			}
		}
		pthread_mutex_unlock(&inf->lock_poll);
	} else if (rc == EBUSY) {
		// there is another thread holding the lock, just wait the thread call us.
		pthread_mutex_lock(&sync_ctx->lock);
		rc = lrpc_call_async(endpoint, async_ctx, method_name, args, args_len, sync_call_finish);
		if (rc == 0) {
			while (sync_ctx->done == 0) {
				pthread_cond_wait(&sync_ctx->cond, &sync_ctx->lock);
			}
		}
		pthread_mutex_unlock(&sync_ctx->lock);
	} else {
		// we can not get the lock, and the lock is not holding by another thread.
		// has the lock already initialized by lrpc_init()?
		errno = rc;
		rc = -1;
	}
	return rc;
}

EXPORT ssize_t lrpc_call(struct lrpc_endpoint *endpoint,
                         const char *method_name, const void *args, size_t args_len,
                         void *ret_ptr, size_t ret_size)
{
	int rc = 0;
	ssize_t size;
	struct lrpc_call_ctx ctx;
	struct sync_call_context sync_ctx;

	pthread_mutex_init(&sync_ctx.lock, NULL);
	pthread_cond_init(&sync_ctx.cond, NULL);

	sync_ctx.ret_ptr = ret_ptr;
	sync_ctx.ret_size = ret_size;
	sync_ctx.err_code = 0;
	sync_ctx.done = 0;
	ctx.user_data = &sync_ctx;
	ctx.cb = sync_call_finish;

	rc = endpoint_call_and_wait(endpoint, &ctx, &sync_ctx, method_name, args, args_len);

	pthread_mutex_destroy(&sync_ctx.lock);
	pthread_cond_destroy(&sync_ctx.cond);

	if (rc < 0) {
		size = -2;
	} else if (sync_ctx.err_code != 0) {
		size = -1;
		errno = sync_ctx.err_code;
	} else {
		size = sync_ctx.ret_size;
	}

	return size;
}

EXPORT int
lrpc_call_async(struct lrpc_endpoint *endpoint, struct lrpc_call_ctx *ctx, const char *method, const void *args,
                size_t args_len, lrpc_async_callback cb)
{
	int rc;
	struct lrpc_interface *inf;
	struct msghdr msg;
	struct iovec iov[MSGIOV_MAX];
	struct lrpc_msg_call call;

	assert(endpoint);
	assert(ctx);
	assert(cb);
	assert(method);
	assert(args_len == 0 || args);

	ctx->cb = cb;

	if (msg_build_call(endpoint, &call, &msg, iov, method, args, args_len) < 0) {
		goto err;
	}
	ctx->cookie = (lrpc_cookie_t) &call;

	inf = endpoint->inf;

	rc = inf_async_call(inf, ctx, &msg);
	if (rc < 0) {
		goto err;
	}
	return 0;
err:
	return -1;
}
