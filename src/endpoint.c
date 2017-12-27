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
#include "config.h"
#include "interface.h"

void endpoint_init(struct lrpc_endpoint *endpoint, struct lrpc_socket *sock, const char *name, size_t name_len)
{
	char *p;

	assert(endpoint && name_len && name);

	endpoint->sock = sock;
	endpoint->addr.sun_family = AF_UNIX;
	p = endpoint->addr.sun_path;
	*p++ = 0;
	memcpy(p, LRPC_MSG_NAME_PREFIX, sizeof(LRPC_MSG_NAME_PREFIX) - 1);
	p += sizeof(LRPC_MSG_NAME_PREFIX) - 1;
	memcpy(p, name, name_len);
	p += name_len;
	endpoint->addr_len = (socklen_t) (offsetof(struct sockaddr_un, sun_path) + (p - endpoint->addr.sun_path));
}

static ssize_t lrpc_invoke(struct lrpc_endpoint *endpoint, struct msghdr *msg, struct msghdr *rmsg)
{
	ssize_t size;
	size = socket_sendmsg(endpoint->sock, msg, MSG_NOSIGNAL);
	if (size < 0) {
		return -1;
	}

	struct lrpc_msg_head *send_head = (struct lrpc_msg_head *) (msg->msg_iov[MSGIOV_HEAD].iov_base),
		*recv_head = (struct lrpc_msg_head *) (rmsg->msg_iov[MSGIOV_HEAD].iov_base);
	size = recvmsg(endpoint->sock->fd, rmsg, MSG_NOSIGNAL);
	if (size < 0) {
		return -1;
	}

	if (size < sizeof(struct lrpc_msg_head)) {
		fprintf(stderr, "protocol mismatched.\n");
		abort();
	}

	if (memcmp(rmsg->msg_name, &endpoint->addr, endpoint->addr_len) != 0) {
		fprintf(stderr, "call/return from multi peer is not support yet.\n");
		abort();
	}

	if (recv_head->cookie != send_head->cookie) {
		fprintf(stderr, "Async call/return is not support yet.\n");
		abort();
	}

	return size;
}

static int build_call_msg(struct lrpc_endpoint *endpoint,
                          struct lrpc_msg_call *call,
                          struct msghdr *msg,
                          struct iovec iov[MSGIOV_MAX],
                          const char *method_name,
                          const void *args, size_t args_len)
{
	size_t method_len;

	method_len = strlen(method_name);
	if (method_len > LRPC_METHOD_NAME_MAX) {
		errno = EINVAL;
		return -1;
	}

	call->head.cookie = (lrpc_cookie_t) call;
	call->head.type = LRPC_MSGTYP_CALL;
	call->head.body_size = (uint16_t) args_len;
	call->method_len = (uint8_t) method_len;
	call->args_len = (uint16_t) args_len;
	memcpy(call->method, method_name, method_len);

	iov[MSGIOV_HEAD].iov_base = call;
	iov[MSGIOV_HEAD].iov_len = sizeof(*call);
	iov[MSGIOV_BODY].iov_base = (void *) args;
	iov[MSGIOV_BODY].iov_len = args_len;

	msg->msg_name = &endpoint->addr;
	msg->msg_namelen = endpoint->addr_len;
	msg->msg_iov = iov;
	msg->msg_iovlen = MSGIOV_MAX;
	msg->msg_control = NULL;
	msg->msg_controllen = 0;
	msg->msg_flags = 0;

	return 0;
}

ssize_t lrpc_call(struct lrpc_endpoint *endpoint,
                  const char *method_name, const void *args, size_t args_len,
                  void *ret_ptr, size_t ret_size)
{
	ssize_t size;
	struct sockaddr_un addr;
	struct msghdr msg, rmsg;
	struct iovec iov[MSGIOV_MAX], riov[MSGIOV_MAX];
	struct lrpc_msg_call call;
	struct lrpc_msg_return returns;

	if (args_len > LRPC_MAX_ARGS_LENGTH) {
		errno = EINVAL;
		return -1;
	}

	if (build_call_msg(endpoint, &call, &msg, iov, method_name, args, args_len) < 0) {
		return -1;
	}

	riov[MSGIOV_HEAD].iov_base = &returns;
	riov[MSGIOV_HEAD].iov_len = sizeof(returns);
	riov[MSGIOV_BODY].iov_base = ret_ptr;
	riov[MSGIOV_BODY].iov_len = ret_size;

	rmsg.msg_name = &addr;
	rmsg.msg_namelen = sizeof(addr);
	rmsg.msg_iov = riov;
	rmsg.msg_iovlen = sizeof(iov) / sizeof(*iov);
	rmsg.msg_control = NULL;
	rmsg.msg_controllen = 0;
	rmsg.msg_flags = 0;

	size = lrpc_invoke(endpoint, &msg, &rmsg);
	return size - sizeof(struct lrpc_msg_return);
}

int lrpc_call_async(struct lrpc_endpoint *endpoint, struct lrpc_async_call_ctx *ctx)
{
	int rc;
	struct lrpc_interface *inf;
	struct msghdr msg;
	struct iovec iov[MSGIOV_MAX];
	struct lrpc_msg_call call;

	assert(endpoint);
	assert(ctx);
	assert(ctx->cb);
	assert(ctx->method);
	assert(ctx->args_len == 0 || ctx->args);

	if (build_call_msg(endpoint, &call, &msg, iov, ctx->method, ctx->args, ctx->args_len) < 0) {
		goto err;
	}
	ctx->cookie = (lrpc_cookie_t) &call;

	inf = endpoint->sock->inf;


	rc = inf_async_call(inf, ctx, &msg);
	if (rc < 0) {
		goto err;
	}
	return 0;
err:
	return -1;
}
