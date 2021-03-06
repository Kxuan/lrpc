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

#ifndef LRPC_MSG_H
#define LRPC_MSG_H

#include <stdint.h>
#include <lrpc.h>

enum msgiov_types
{
	MSGIOV_HEAD,
	MSGIOV_BODY,
	MSGIOV_MAX
};

enum lrpc_msg_types
{
	LRPC_MSGTYP_INVOKE,
	LRPC_MSGTYP_RETURN,
	LRPC_MSGTYP_RETURN_ERROR
};

struct lrpc_msg_head
{
	lrpc_cookie_t cookie;
	uint16_t body_size;
	uint8_t type; //enum lrpc_msg_types
};

enum lrpc_call_ret_status
{
	LRPC_RETST_CALLBACK,
	LRPC_RETST_ASYNC,
	LRPC_RETST_DONE
};

struct lrpc_msg_invoke
{
	struct lrpc_msg_head head;

	uint16_t args_len;
	uint8_t func_len;
	// CLOCK_MONOTONIC timeout value
	struct timespec timeout;
	char func[LRPC_FUNC_NAME_MAX];
	char args[0];
};

struct lrpc_invoke_ctx
{
	struct lrpc_interface *inf;
	struct lrpc_packet *pkt;
	uint8_t ret_status;
};

struct lrpc_msg_return
{
	struct lrpc_msg_head head;
	uint16_t returns_len;
};

int msg_build_invoke(const struct lrpc_endpoint *endpoint,
                     struct lrpc_msg_invoke *buf,
                     struct msghdr *msg,
                     const struct lrpc_invoke_req *ctx);

#endif //LRPC_MSG_H
