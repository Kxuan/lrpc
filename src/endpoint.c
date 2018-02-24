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

EXPORT int lrpc_endpoint_name(const struct lrpc_endpoint *endpoint, const char **name, size_t *name_len)
{
	*name = endpoint->addr.sun_path + sizeof(LRPC_MSG_NAME_PREFIX);
	*name_len = endpoint->addr_len - offsetof(struct sockaddr_un, sun_path) - sizeof(LRPC_MSG_NAME_PREFIX);
	return 0;
}

struct sync_call_context
{
	int done;
	int err_code;

	void *ret;
	size_t ret_size;

	pthread_mutex_t lock;
	pthread_cond_t cond;
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
		memcpy(sync_ctx->ret, ret_ptr, copy_size);
		sync_ctx->ret_size = copy_size;
	}
	sync_ctx->done = 1;
	pthread_cond_broadcast(&sync_ctx->cond);
	pthread_mutex_unlock(&sync_ctx->lock);
}

EXPORT int lrpc_call(struct lrpc_endpoint *endpoint,
                     const char *func_name, const void *args, size_t args_len,
                     void *ret_ptr, size_t *ret_size)
{
	assert(ret_ptr != NULL && ret_size != NULL || ret_ptr == NULL && ret_size == NULL);

	int rc;
	struct lrpc_call_ctx ctx;
	struct sync_call_context sync_ctx;

	pthread_mutex_init(&sync_ctx.lock, NULL);
	pthread_cond_init(&sync_ctx.cond, NULL);

	sync_ctx.err_code = 0;
	sync_ctx.done = 0;
	sync_ctx.ret = ret_ptr;
	sync_ctx.ret_size = ret_size ? *ret_size : 0;

	ctx.user_data = &sync_ctx;
	ctx.cb = sync_call_finish;
	ctx.func = func_name;
	ctx.args = args;
	ctx.args_size = args_len;

	pthread_mutex_lock(&sync_ctx.lock);
	rc = lrpc_call_async(endpoint, &ctx);
	if (rc == 0) {
		while (sync_ctx.done == 0) {
			pthread_cond_wait(&sync_ctx.cond, &sync_ctx.lock);
		}
	}
	pthread_mutex_unlock(&sync_ctx.lock);

	pthread_mutex_destroy(&sync_ctx.lock);
	pthread_cond_destroy(&sync_ctx.cond);

	if (rc < 0) {
		goto out;
	}

	if (sync_ctx.err_code != 0) {
		rc = -1;
		errno = sync_ctx.err_code;
		goto out;
	}

	if (ret_size) {
		*ret_size = sync_ctx.ret_size;
	}

out:
	return rc;
}

EXPORT int lrpc_call_async(struct lrpc_endpoint *endpoint, struct lrpc_call_ctx *ctx)
{
	int rc;
	struct lrpc_interface *inf;
	struct iovec iov[MSGIOV_MAX];
	struct msghdr msg = {.msg_iov = iov};
	struct lrpc_msg_call buf;

	assert(endpoint);
	assert(ctx);
	assert(ctx->cb);
	assert(ctx->func);
	assert(ctx->args_size == 0 || ctx->args);


	ctx->cookie = (lrpc_cookie_t) ctx;
	rc = msg_build_call(endpoint, &buf, &msg, ctx);
	if (rc < 0) {
		goto err;
	}

	inf = endpoint->inf;

	rc = inf_async_call(inf, ctx, &msg);
	if (rc < 0) {
		goto err;
	}
	return 0;
err:
	return -1;
}
