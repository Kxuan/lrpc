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
#include "msg.h"
#include "lrpc.h"

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

int lrpc_connect(struct lrpc_interface *inf,
                 struct lrpc_endpoint *endpoint, const char *name, size_t name_len)
{
	endpoint_init(endpoint, &inf->sock, name, name_len);
	return 0;
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

int lrpc_stop(struct lrpc_interface *inf)
{
	lrpc_socket_close(&inf->sock);
}

int lrpc_poll(struct lrpc_interface *inf)
{
	struct lrpc_packet *msg;
	msg = lrpc_socket_recvmsg(&inf->sock, 0);
	if (!msg) {
		return -1;
	}

	lrpc_msg_feed(inf, msg);

	return 0;
}