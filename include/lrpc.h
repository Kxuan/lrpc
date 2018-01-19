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

#define LRPC_MAX_PACKET_SIZE (512)
#define LRPC_METHOD_NAME_MAX (32)


typedef uintptr_t lrpc_cookie_t;

struct lrpc_endpoint
{
	struct lrpc_interface *inf;
	socklen_t addr_len;
	struct sockaddr_un addr;
};

struct lrpc_interface;

struct lrpc_packet
{
	struct sockaddr_un addr;
	struct msghdr msgh;
	struct iovec iov;
	size_t payload_len;
	char payload[LRPC_MAX_PACKET_SIZE];
};

struct lrpc_callback_ctx;

typedef int (*lrpc_method_cb)(void *user_data, const struct lrpc_callback_ctx *pkt, void *args, size_t args_len);

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
	int fd;
	struct lrpc_endpoint local_endpoint;
	struct method_table all_methods;

	pthread_mutex_t lock_call_list;
	struct lrpc_async_call_ctx *call_list;
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

	lrpc_async_callback cb;

	/* Private field */
	lrpc_cookie_t cookie;
	struct lrpc_async_call_ctx *prev, *next;
};

ssize_t lrpc_call(struct lrpc_endpoint *endpoint,
                  const char *method_name, const void *args, size_t args_len,
                  void *ret_ptr, size_t ret_size);

int lrpc_call_async(struct lrpc_endpoint *endpoint, struct lrpc_async_call_ctx *ctx, const char *method, const void *args,
                    size_t args_len, lrpc_async_callback cb);

void lrpc_init(struct lrpc_interface *inf, char *name, size_t name_len);

int lrpc_start(struct lrpc_interface *inf);

int lrpc_stop(struct lrpc_interface *inf);

int lrpc_poll(struct lrpc_interface *inf, struct lrpc_packet *buffer);

int lrpc_method(struct lrpc_interface *inf, struct lrpc_method *method);

int lrpc_connect(struct lrpc_interface *inf,
                 struct lrpc_endpoint *endpoint, const char *name, size_t name_len);

void lrpc_method_init(struct lrpc_method *method, const char *name, lrpc_method_cb callback, void *user_data);

struct lrpc_method *lrpc_method_create(const char *name, lrpc_method_cb callback, void *user_data);

int lrpc_return_async(const struct lrpc_callback_ctx *ctx, struct lrpc_async_return_ctx *async_ctx);

int lrpc_return_finish(struct lrpc_async_return_ctx *ctx, const void *ret, size_t ret_size);

int lrpc_return(const struct lrpc_callback_ctx *ctx, const void *ret, size_t ret_size);

int lrpc_msg_feed(struct lrpc_interface *inf, struct lrpc_packet *msg);

#endif //LRPC_LRPC_H
