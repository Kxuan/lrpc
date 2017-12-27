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


#include <sys/un.h>
#include <utlist.h>
#include <lrpc.h>
#include <stdio.h>
#include "interface.h"
#include "socket.h"
#include "msg.h"

int inf_async_call(struct lrpc_interface *inf, struct lrpc_async_call_ctx *ctx, struct msghdr *msg)
{
	ssize_t size;
	DL_APPEND(inf->async_call_list, ctx);
	size = socket_sendmsg(&inf->sock, msg, 0);
	if (size <= 0) {
		goto err;
	}
	return 0;
err:
	DL_DELETE(inf->async_call_list, ctx);
	return -1;
}

int lrpc_connect(struct lrpc_interface *inf,
                 struct lrpc_endpoint *endpoint, const char *name, size_t name_len)
{
	endpoint_init(endpoint, &inf->sock, name, name_len);
	return 0;
}

int lrpc_method(struct lrpc_interface *inf, struct lrpc_method *method)
{
	return method_register(&inf->all_methods, method);
}

void lrpc_init(struct lrpc_interface *inf, char *name, size_t name_len)
{
	inf->async_call_list = NULL;
	lrpc_socket_init(&inf->sock, inf, name, name_len);
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

int lrpc_poll(struct lrpc_interface *inf, struct lrpc_packet *msg)
{
	int rc;
	rc = socket_recvmsg(&inf->sock, msg, 0);
	if (rc < 0) {
		return -1;
	}

	rc = lrpc_msg_feed(inf, msg);
	return rc;
}