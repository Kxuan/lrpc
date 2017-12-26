/* Copyright (C) 2017 kXuan <kxuanobj@gmail.com>

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
   <http://www.gnu.org/licenses/>.  */


#include <linux/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "interface.h"
#include "socket.h"

static int lrpc_invoke(struct lrpc_endpoint *endpoint, struct msghdr *msg, struct msghdr *rmsg)
{
	ssize_t size;
	size = sendmsg(endpoint->sock->fd, msg, MSG_NOSIGNAL);
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

	return 0;
}

int lrpc_call(struct lrpc_endpoint *endpoint,
              const char *method_name, const void *args, size_t args_len,
              void *rc_ptr, size_t rc_size)
{
	int rc;
	size_t method_len;
	struct sockaddr_un addr;
	struct msghdr msg, rmsg;
	struct iovec iov[MSGIOV_MAX], riov[MSGIOV_MAX];
	struct lrpc_msg_call call;
	struct lrpc_msg_return returns;

	if (args_len > LRPC_MAX_ARGS_LENGTH) {
		errno = EINVAL;
		return -1;
	}

	method_len = strlen(method_name);
	if (method_len > LRPC_METHOD_NAME_MAX) {
		errno = EINVAL;
		return -1;
	}

	call.head.cookie = (uintptr_t) &call;
	call.head.type = LRPC_MSGTYP_CALL;
	call.head.body_size = args_len;
	call.method_len = (uint8_t) method_len;
	call.args_len = (uint16_t) args_len;
	memcpy(call.method, method_name, method_len);

	iov[MSGIOV_HEAD].iov_base = &call;
	iov[MSGIOV_HEAD].iov_len = sizeof(call);
	iov[MSGIOV_BODY].iov_base = (void *) args;
	iov[MSGIOV_BODY].iov_len = args_len;

	msg.msg_name = &endpoint->addr;
	msg.msg_namelen = endpoint->addr_len;
	msg.msg_iov = iov;
	msg.msg_iovlen = sizeof(iov) / sizeof(*iov);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	riov[MSGIOV_HEAD].iov_base = &returns;
	riov[MSGIOV_HEAD].iov_len = sizeof(returns);
	riov[MSGIOV_BODY].iov_base = rc_ptr;
	riov[MSGIOV_BODY].iov_len = rc_size;

	rmsg.msg_name = &addr;
	rmsg.msg_namelen = sizeof(addr);
	rmsg.msg_iov = riov;
	rmsg.msg_iovlen = sizeof(iov) / sizeof(*iov);
	rmsg.msg_control = NULL;
	rmsg.msg_controllen = 0;
	rmsg.msg_flags = 0;

	rc = lrpc_invoke(endpoint, &msg, &rmsg);

	return rc;
}

static int lrpc_do_return(struct lrpc_socket *sock,
                          const struct sockaddr_un *msg_from, socklen_t msg_from_len,
                          struct lrpc_msg_call *call,
                          enum lrpc_msg_types status, void *body, size_t body_len)
{
	struct msghdr msg;
	struct iovec iov[2];
	struct lrpc_msg_return r;
	ssize_t size;

	r.head.cookie = call->head.cookie;
	r.head.type = status;
	r.head.body_size = sizeof(int);
	r.returns_len = r.head.body_size;

	iov[0].iov_base = &r;
	iov[0].iov_len = sizeof(r);
	iov[1].iov_base = body;
	iov[1].iov_len = body_len;

	msg.msg_name = (void *) msg_from;
	msg.msg_namelen = msg_from_len;
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

static int lrpc_return_error(struct lrpc_interface *inf,
                             const struct sockaddr_un *msg_from, socklen_t msg_from_len,
                             struct lrpc_msg_call *call, int err)
{
	return lrpc_do_return(&inf->sock, msg_from, msg_from_len, call, LRPC_MSGTYP_RETURN_ERROR, &err, sizeof(err));
}

static int lrpc_return_val(struct lrpc_interface *inf,
                           const struct sockaddr_un *msg_from, socklen_t msg_from_len,
                           struct lrpc_msg_call *call, void *ret, size_t ret_size)
{
	return lrpc_do_return(&inf->sock, msg_from, msg_from_len, call, LRPC_MSGTYP_RETURN_VAL, ret, ret_size);
}


static int lrpc_feed_msg_call(struct lrpc_interface *inf,
                              const struct sockaddr_un *msg_from, socklen_t msg_from_len,
                              struct lrpc_msg_call *call, void *args, size_t args_len)
{
	struct lrpc_method *method;
	struct lrpc_msg_return *ret;
	size_t ret_size;
	lrpc_return_status ret_status;


	if (call->method_len > sizeof(call->method) || call->args_len != args_len) {
		errno = EPROTO;
		return -1;
	}

	method = method_find(&inf->all_methods, call->method, call->method_len);
	if (method == NULL) {
		lrpc_return_error(inf, msg_from, msg_from_len, call, EOPNOTSUPP);
		errno = EINVAL;
		return -1;
	}

	ret = (struct lrpc_msg_return *) inf->sock.send_buf;
	ret_size = LRPC_MAX_PACKET_SIZE;

	ret_status = method->callback(method->user_data, call + 1, call->args_len, ret, &ret_size);

	switch (ret_status) {
	case LRPC_RETURN_VAL: {
		ret->returns_len = (uint16_t) ret_size;
		assert(ret_size <= LRPC_MAX_PACKET_SIZE);

		lrpc_return_val(inf, msg_from, msg_from_len, call, ret, ret_size);

		break;
	}
	case LRPC_RETURN_ERROR:
		lrpc_return_error(inf, msg_from, msg_from_len, call, errno);
		break;
	}

}

static int lrpc_feed_msg(struct lrpc_interface *inf, struct msghdr *msg, size_t msg_size)
{
	struct lrpc_msg_head *head;
	int rc = -1;

	head = msg->msg_iov[0].iov_base;

	if (sizeof(*head) + head->body_size > msg_size) {
		errno = EPROTO;
		return -1;
	}

	switch (head->type) {
	case LRPC_MSGTYP_CALL:
		rc = lrpc_feed_msg_call(inf, msg->msg_name, msg->msg_namelen, (struct lrpc_msg_call *) head, head + 1,
		                        msg_size - sizeof(struct lrpc_msg_call));
		break;
	case LRPC_MSGTYP_RETURN_VAL:
	case LRPC_MSGTYP_RETURN_ERROR:
		fprintf(stderr, "LRPC_MSGTYP_RETURN_VAL/LRPC_MSGTYP_RETURN_ERROR is not support yet.\n");
		abort();
		break;
	default:
		break;
	}
	return rc;
}

int lrpc_connect(struct lrpc_interface *inf,
                 struct lrpc_endpoint *endpoint, const char *name, size_t name_len)
{
	return endpoint_open(&inf->sock, endpoint, name, name_len);
}

int lrpc_register_method(struct lrpc_interface *inf, struct lrpc_method *method)
{
	return method_register(&inf->all_methods, method);
}

void lrpc_init(struct lrpc_interface *inf, char *name, size_t name_len)
{
	lrpc_socket_init(&inf->sock, name, name_len);
	method_table_init(&inf->all_methods);
}

int lrpc_start(struct lrpc_interface *inf)
{
	return lrpc_socket_open(&inf->sock);
}

int lrpc_poll(struct lrpc_interface *inf)
{
	struct sockaddr_un addr;
	struct msghdr msg;
	struct iovec iov;
	ssize_t size;

	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	size = lrpc_socket_recvmsg(&inf->sock, &msg, 0);
	if (size <= 0) {
		return -1;
	}

	lrpc_feed_msg(inf, &msg, size);

	return 0;
}