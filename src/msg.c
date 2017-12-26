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

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include "lrpc-internal.h"
#include "msg.h"
#include "socket.h"


static int lrpc_do_return(struct lrpc_socket *sock,
                          const struct sockaddr_un *addr, socklen_t addr_len,
                          struct lrpc_msg_call *call,
                          enum lrpc_msg_types types,
                          const void *body, size_t body_len)
{
	struct msghdr msg;
	struct iovec iov[2];
	struct lrpc_msg_return r;
	ssize_t size;

	r.head.cookie = call->head.cookie;
	r.head.type = types;
	r.head.body_size = sizeof(int);
	r.returns_len = r.head.body_size;

	iov[0].iov_base = &r;
	iov[0].iov_len = sizeof(r);
	iov[1].iov_base = (void *) body;
	iov[1].iov_len = body_len;

	msg.msg_name = (void *) addr;
	msg.msg_namelen = addr_len;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = sizeof(iov) / sizeof(*iov);
	msg.msg_flags = 0;

	size = sendmsg(sock->fd, &msg, MSG_NOSIGNAL);
	if (size < 0) {
		return -1;
	}
	return 0;
}

static int lrpc_return_error(struct lrpc_socket *sock, struct lrpc_packet *pkt, int err)
{
	return lrpc_do_return(sock,
	                      &pkt->addr, pkt->msgh.msg_namelen,
	                      (struct lrpc_msg_call *) pkt->payload,
	                      LRPC_MSGTYP_RETURN_ERROR,
	                      &err, sizeof(err));

}

int lrpc_return_async(const void *user_ctx)
{
	struct lrpc_ctx_call *ctx;
	ctx = (struct lrpc_ctx_call *) user_ctx;

	if (ctx->ret_status != LRPC_RETST_CALLBACK) {
		errno = EINVAL;
		return -1;
	}

	ctx->ret_status = LRPC_RETST_ASYNC;
	return 0;
}

int lrpc_return(const void *user_ctx, const void *ret, size_t ret_size)
{
	int rc = -1;
	struct lrpc_ctx_call *ctx;
	struct lrpc_packet *pkt;

	ctx = (struct lrpc_ctx_call *) user_ctx;
	pkt = container_of(ctx, struct lrpc_packet, context);

	switch (ctx->ret_status) {
	case LRPC_RETST_CALLBACK:
		rc = lrpc_do_return(&ctx->inf->sock,
		                    &pkt->addr, pkt->msgh.msg_namelen,
		                    (struct lrpc_msg_call *) pkt->payload,
		                    LRPC_MSGTYP_RETURN,
		                    ret, ret_size);
		ctx->ret_status = LRPC_RETST_DONE;
		break;
	case LRPC_RETST_ASYNC:
		rc = lrpc_do_return(&ctx->inf->sock,
		                    &pkt->addr, pkt->msgh.msg_namelen,
		                    (struct lrpc_msg_call *) pkt->payload,
		                    LRPC_MSGTYP_RETURN,
		                    ret, ret_size);
		lrpc_socket_freemsg(pkt);
		break;
	default:
		errno = EINVAL;
		rc = -1;
		break;
	}
	return rc;
}


static int feed_msg_call(struct lrpc_interface *inf, struct lrpc_packet *msg)
{
	struct lrpc_method *method;
	struct lrpc_ctx_call *ctx;
	struct lrpc_msg_call *call;
	int cb_returns;

	ctx = (struct lrpc_ctx_call *) msg->context;
	call = (struct lrpc_msg_call *) msg->payload;

	if (msg->payload_len < sizeof(*call) ||
	    call->method_len > sizeof(call->method) ||
	    call->args_len != msg->payload_len - sizeof(*call)) {
		errno = EPROTO;
		return -1;
	}

	method = method_find(&inf->all_methods, call->method, call->method_len);
	if (method == NULL) {
		lrpc_return_error(&inf->sock, msg, EOPNOTSUPP);
		errno = EINVAL;
		return -1;
	}

	ctx->inf = inf;
	ctx->ret_status = LRPC_RETST_CALLBACK;
	cb_returns = method->callback(method->user_data, ctx, call + 1, msg->payload_len - sizeof(*call));

	switch (ctx->ret_status) {
	case LRPC_RETST_CALLBACK:
		lrpc_return(ctx, &cb_returns, sizeof(cb_returns));
		break;
	case LRPC_RETST_ASYNC:
		break;
	case LRPC_RETST_DONE:
	default:
		lrpc_socket_freemsg(msg);
		break;
	}
	return 0;
}

int lrpc_msg_feed(struct lrpc_interface *inf, struct lrpc_packet *msg)
{
	struct lrpc_msg_head *head;
	int rc = -1;

	head = msg->msgh.msg_iov[0].iov_base;

	if (sizeof(*head) + head->body_size > msg->payload_len) {
		errno = EPROTO;
		return -1;
	}

	switch (head->type) {
	case LRPC_MSGTYP_CALL:
		rc = feed_msg_call(inf, msg);
		break;
	case LRPC_MSGTYP_RETURN:
		fprintf(stderr, "LRPC_MSGTYP_RETURN is not support yet.\n");
		abort();
		break;
	default:
		break;
	}
	return rc;
}