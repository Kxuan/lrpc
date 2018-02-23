/* Copyright (C) 2017-2018 kXuan <kxuanobj@gmail.com>

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
#include <pthread.h>
#include <errno.h>
#include "lrpc-internal.h"
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

EXPORT ssize_t lrpc_call(struct lrpc_endpoint *endpoint,
                         const char *func_name, const void *args, size_t args_len,
                         void *ret_ptr, size_t ret_size)
{
	int tmp;
	ssize_t rc;
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

	pthread_mutex_lock(&sync_ctx.lock);
	tmp = lrpc_call_async(endpoint, &ctx, func_name, args, args_len, sync_call_finish);
	if (tmp == 0) {
		while (sync_ctx.done == 0) {
			pthread_cond_wait(&sync_ctx.cond, &sync_ctx.lock);
		}
	}
	pthread_mutex_unlock(&sync_ctx.lock);

	pthread_mutex_destroy(&sync_ctx.lock);
	pthread_cond_destroy(&sync_ctx.cond);

	if (tmp < 0) {
		rc = -2;
	} else if (sync_ctx.err_code != 0) {
		rc = -1;
		errno = sync_ctx.err_code;
	} else {
		rc = sync_ctx.ret_size;
	}

	return rc;
}

EXPORT int lrpc_call_async(struct lrpc_endpoint *endpoint, struct lrpc_call_ctx *ctx, const char *func,
                           const void *args, size_t args_len, lrpc_async_callback cb)
{
	int rc;
	struct lrpc_interface *inf;
	struct iovec iov[MSGIOV_MAX];
	struct msghdr msg = {.msg_iov = iov};
	struct lrpc_msg_call call;

	assert(endpoint);
	assert(ctx);
	assert(cb);
	assert(func);
	assert(args_len == 0 || args);

	ctx->cb = cb;

	rc = msg_build_call(endpoint, &call, &msg, func, args, args_len);
	if (rc < 0) {
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
