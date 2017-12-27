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

#ifndef LRPC_LRPC_H
#define LRPC_LRPC_H

#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>

#define LRPC_MAX_ARGS_LENGTH UINT16_MAX

typedef uintptr_t lrpc_cookie_t;

struct lrpc_socket;
struct lrpc_endpoint
{
	struct lrpc_socket *sock;
	socklen_t addr_len;
	struct sockaddr_un addr;
};

struct lrpc_interface;
struct lrpc_socket
{
	struct lrpc_interface *inf;
	int fd;
	struct lrpc_endpoint endpoint;
};

struct lrpc_packet;

typedef int (*lrpc_method_cb)(void *user_data, const struct lrpc_packet *pkt, void *args, size_t args_len);

struct lrpc_method
{
	const char *name;
	lrpc_method_cb callback;
	void *user_data;

	struct lrpc_method *prev, *next;
};


struct method_table
{
	struct lrpc_method *all_methods;
};

struct lrpc_async_call_ctx;
struct lrpc_interface
{
	struct lrpc_socket sock;
	struct method_table all_methods;
	// todo MT-safe
	struct lrpc_async_call_ctx *async_call_list;
};

struct lrpc_async_return_ctx
{
	struct lrpc_interface *inf;
	struct sockaddr_un addr;
	socklen_t addr_len;
	lrpc_cookie_t cookie;
};

typedef void (*lrpc_async_callback)(struct lrpc_async_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size);

struct lrpc_async_call_ctx
{
	void *user_data;

	const char *method;
	const char *args;
	size_t args_len;

	lrpc_async_callback cb;

	/* Private field */
	lrpc_cookie_t cookie;
	struct lrpc_async_call_ctx *prev, *next;
};

#define LRPC_METHOD_NAME_MAX (32)

#endif //LRPC_LRPC_H
