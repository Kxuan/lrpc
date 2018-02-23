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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <utlist.h>
#include "lrpc-internal.h"
#include "msg.h"
#include "func_table.h"


int msg_build_call(struct lrpc_endpoint *endpoint,
                   struct lrpc_msg_call *call,
                   struct msghdr *msg,
                   const char *func_name,
                   const void *args, size_t args_len)
{
	size_t func_len;
	struct iovec *iov = msg->msg_iov;

	func_len = strlen(func_name);
	if (func_len > LRPC_METHOD_NAME_MAX) {
		errno = EINVAL;
		return -1;
	}

	call->head.cookie = (lrpc_cookie_t) call;
	call->head.type = LRPC_MSGTYP_CALL;
	call->head.body_size = (uint16_t) args_len;
	call->func_len = (uint8_t) func_len;
	call->args_len = (uint16_t) args_len;
	memcpy(call->func, func_name, func_len);

	iov[MSGIOV_HEAD].iov_base = call;
	iov[MSGIOV_HEAD].iov_len = sizeof(*call);
	iov[MSGIOV_BODY].iov_base = (void *) args;
	iov[MSGIOV_BODY].iov_len = args_len;

	msg->msg_name = &endpoint->addr;
	msg->msg_namelen = endpoint->addr_len;
	msg->msg_iovlen = MSGIOV_MAX;
	msg->msg_control = NULL;
	msg->msg_controllen = 0;
	msg->msg_flags = 0;

	return 0;
}

static int lrpc_do_return(struct lrpc_interface *inf,
                          const struct sockaddr_un *addr, socklen_t addr_len,
                          lrpc_cookie_t cookie,
                          enum lrpc_msg_types types,
                          const void *body, size_t body_len)
{
	struct msghdr msg;
	struct iovec iov[2];
	struct lrpc_msg_return r;
	ssize_t size;

	r.head.cookie = cookie;
	r.head.type = types;
	r.head.body_size = (uint16_t) body_len;
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

	size = sendmsg(inf->fd, &msg, MSG_NOSIGNAL);
	if (size < 0) {
		return -1;
	}
	return 0;
}

static int lrpc_return_error(struct lrpc_interface *inf, struct lrpc_packet *pkt, int err)
{
	struct lrpc_msg_call *call;
	call = (struct lrpc_msg_call *) pkt->payload;
	return lrpc_do_return(inf,
	                      &pkt->addr, pkt->msgh.msg_namelen,
	                      call->head.cookie,
	                      LRPC_MSGTYP_RETURN_ERROR,
	                      &err, sizeof(err));

}

EXPORT int lrpc_return_async(const struct lrpc_callback_ctx *user_ctx, struct lrpc_return_ctx *async_ctx)
{
	struct lrpc_callback_ctx *ctx;
	struct lrpc_packet *pkt;
	struct lrpc_msg_call *call;

	ctx = (struct lrpc_callback_ctx *) user_ctx;
	pkt = ctx->pkt;
	call = (struct lrpc_msg_call *) pkt->payload;

	if (ctx->ret_status != LRPC_RETST_CALLBACK) {
		errno = EINVAL;
		return -1;
	}

	async_ctx->inf = ctx->inf;
	async_ctx->cookie = call->head.cookie;
	async_ctx->addr = pkt->addr;
	async_ctx->addr_len = pkt->msgh.msg_namelen;

	ctx->ret_status = LRPC_RETST_ASYNC;
	return 0;
}

EXPORT int lrpc_return_finish(struct lrpc_return_ctx *ctx, const void *ret, size_t ret_size)
{
	return lrpc_do_return(ctx->inf,
	                      &ctx->addr, ctx->addr_len,
	                      ctx->cookie,
	                      LRPC_MSGTYP_RETURN,
	                      ret, ret_size);
}

EXPORT int lrpc_return(const struct lrpc_callback_ctx *user_ctx, const void *ret, size_t ret_size)
{
	int rc;
	struct lrpc_callback_ctx *ctx;
	struct lrpc_packet *pkt;
	struct lrpc_msg_call *call;

	assert(user_ctx != NULL);
	assert(ret_size == 0 || ret != NULL);

	ctx = (struct lrpc_callback_ctx *) user_ctx;
	pkt = ctx->pkt;
	call = (struct lrpc_msg_call *) pkt->payload;

	if (ctx->ret_status != LRPC_RETST_CALLBACK) {
		errno = EINVAL;
		return -1;
	}

	rc = lrpc_do_return(ctx->inf,
	                    &pkt->addr, pkt->msgh.msg_namelen,
	                    call->head.cookie,
	                    LRPC_MSGTYP_RETURN,
	                    ret, ret_size);

	if (rc == 0) {
		ctx->ret_status = LRPC_RETST_DONE;
	}

	return rc;
}

EXPORT int lrpc_get_args(const struct lrpc_callback_ctx *ctx, void **pargs, size_t *args_len)
{
	struct lrpc_msg_call *call = (struct lrpc_msg_call *) ctx->pkt->payload;
	if (pargs) {
		*pargs = call->args;
	}

	if (args_len) {
		*args_len = ctx->pkt->payload_len - offsetof(struct lrpc_msg_call, args);
	}
	return 0;
}

EXPORT int lrpc_get_ucred(const struct lrpc_callback_ctx *ctx, struct lrpc_ucred *ucred)
{
	struct lrpc_packet *p = ctx->pkt;
	struct ucred *c = NULL;
	struct cmsghdr *cm = CMSG_FIRSTHDR(&p->msgh);
	while (cm != NULL) {
		if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_CREDENTIALS) {
			c = (struct ucred *) CMSG_DATA(cm);
			break;
		}
		cm = CMSG_NXTHDR(&p->msgh, cm);
	}

	if (!c) {
		errno = ENOTSUP;
		return -1;
	}

	ucred->pid = c->pid;
	ucred->gid = c->gid;
	ucred->uid = c->uid;
	return 0;
}

static int feed_msg_call(struct lrpc_interface *inf, struct lrpc_packet *pkt)
{
	struct lrpc_func *func;
	struct lrpc_callback_ctx ctx;
	struct lrpc_msg_call *call;
	int cb_returns;

	call = (struct lrpc_msg_call *) pkt->payload;

	if (pkt->payload_len < sizeof(*call) ||
	    call->func_len > sizeof(call->func) ||
	    call->args_len != pkt->payload_len - sizeof(*call)) {
		errno = EPROTO;
		return -1;
	}

	func = func_find(&inf->all_funcs, call->func, call->func_len);
	if (func == NULL) {
		lrpc_return_error(inf, pkt, EOPNOTSUPP);
		errno = EINVAL;
		return -1;
	}

	ctx.inf = inf;
	ctx.pkt = pkt;
	ctx.ret_status = LRPC_RETST_CALLBACK;
	cb_returns = func->callback(func->user_data, &ctx);

	if (ctx.ret_status == LRPC_RETST_CALLBACK) {
		lrpc_return(&ctx, &cb_returns, sizeof(cb_returns));
	}
	return 0;
}

static int feed_msg_return(struct lrpc_interface *inf, struct lrpc_packet *pkt)
{
	struct lrpc_msg_return *returns;
	void *ret_data = NULL;
	size_t ret_len = 0;
	int err_code = 0;

	returns = (struct lrpc_msg_return *) pkt->payload;

	if (pkt->payload_len < sizeof(struct lrpc_msg_return) ||
	    pkt->payload_len != returns->returns_len + sizeof(struct lrpc_msg_return) ||
	    (returns->head.type == LRPC_MSGTYP_RETURN_ERROR && returns->returns_len != sizeof(errno))) {
		return -1;
	}

	struct lrpc_call_ctx *ctx;
	pthread_mutex_lock(&inf->lock_call_list);
	DL_FOREACH(inf->call_list, ctx) {
		if (ctx->cookie == returns->head.cookie) {
			DL_DELETE(inf->call_list, ctx);
			break;
		}
	}
	pthread_mutex_unlock(&inf->lock_call_list);
	if (!ctx) {
		return -1;
	}


	if (returns->head.type == LRPC_MSGTYP_RETURN_ERROR) {
		err_code = *(int *) (returns + 1);
	} else {
		ret_data = returns + 1;
		ret_len = returns->returns_len;
	}

	ctx->cb(ctx, err_code, ret_data, ret_len);
	return 0;

}

/**
 * feed message to the interface
 *
 * @param inf the interface
 * @param msg message to feed
 * @return 0 on success, -1 on failure
 */
int lrpc_msg_feed(struct lrpc_interface *inf, struct lrpc_packet *msg)
{
	struct lrpc_msg_head *head;
	int rc = -1;

	head = msg->msgh.msg_iov[0].iov_base;

	if (sizeof(*head) > msg->payload_len ||
	    sizeof(*head) + head->body_size > msg->payload_len) {
		errno = EPROTO;
		return -1;
	}

	switch (head->type) {
	case LRPC_MSGTYP_CALL:
		rc = feed_msg_call(inf, msg);
		break;
	case LRPC_MSGTYP_RETURN:
	case LRPC_MSGTYP_RETURN_ERROR:
		rc = feed_msg_return(inf, msg);
		break;
	default:
		break;
	}
	return rc;
}
