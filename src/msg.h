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

#ifndef LRPC_MSG_H
#define LRPC_MSG_H

#include <stdint.h>

enum lrpc_msg_types
{
	LRPC_MSGTYP_CALL,
	LRPC_MSGTYP_RETURN,
	LRPC_MSGTYP_RETURN_ERROR
};

struct lrpc_msg_head
{
	uintptr_t cookie;
	uint16_t body_size;
	uint8_t type; //enum lrpc_msg_types
};

enum lrpc_call_ret_status
{
	LRPC_RETST_CALLBACK,
	LRPC_RETST_ASYNC,
	LRPC_RETST_DONE
};

#include "method.h"
struct lrpc_msg_call
{
	struct lrpc_msg_head head;

	uint16_t args_len;
	uint8_t method_len;
	char method[LRPC_METHOD_NAME_MAX];
};

struct lrpc_ctx_call
{
	struct lrpc_interface *inf;
	uint8_t ret_status;
};

struct lrpc_msg_return
{
	struct lrpc_msg_head head;
	uint16_t returns_len;
};

union lrpc_msg_ctx
{
	struct lrpc_ctx_call call;
};

#include "interface.h"
#include "socket.h"
int lrpc_return_async(const void *user_ctx);

int lrpc_return(const void *ctx, const void *ret, size_t ret_size);

int lrpc_msg_feed(struct lrpc_interface *inf, struct lrpc_packet *msg);

#endif //LRPC_MSG_H
