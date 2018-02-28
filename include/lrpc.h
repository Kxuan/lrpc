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

#ifndef LRPC_LRPC_H
#define LRPC_LRPC_H

#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>

#define LRPC_DEFAULT_PACKET_SIZE (4096)
#define LRPC_FUNC_NAME_MAX (32)

typedef uintptr_t lrpc_cookie_t;

struct lrpc_ucred
{
	pid_t pid;
	uid_t uid;
	gid_t gid;
};
struct lrpc_cmsg_ucred
{
	struct cmsghdr h;
	struct lrpc_ucred ucred;
};
struct lrpc_packet
{
	struct lrpc_packet *prev, *next;
	struct sockaddr_un addr;
	struct msghdr msgh;
	struct iovec iov;
	size_t payload_len, payload_size;
	char cmsg[sizeof(struct lrpc_cmsg_ucred)];
	char payload[LRPC_DEFAULT_PACKET_SIZE];
};

struct lrpc_invoke_ctx;

typedef int (*lrpc_func_cb)(void *user_data, const struct lrpc_invoke_ctx *ctx);

struct lrpc_func
{
	const char *name;
	lrpc_func_cb callback;
	void *user_data;

	struct lrpc_func *prev, *next;
};

struct func_table
{
	struct lrpc_func *all_funcs;
};

struct lrpc_invoke_req;

typedef void (*lrpc_ivk_ret_cb)(struct lrpc_invoke_req *ctx, int err_code, void *ret_ptr, size_t ret_size);

struct lrpc_invoke_req
{
	lrpc_ivk_ret_cb cb;
	const char *func;

	const void *args;
	size_t args_size;

	void *user_data;

	/* Private field */
	lrpc_cookie_t cookie;
	struct lrpc_invoke_req *prev, *next;
};

struct lrpc_interface;

struct lrpc_endpoint
{
	struct lrpc_interface *inf;
	socklen_t addr_len;
	struct sockaddr_un addr;
};

struct lrpc_interface
{
	int fd;
	struct lrpc_endpoint local_endpoint;
	struct func_table all_funcs;

	pthread_mutex_t lock_invoke_reqs;
	struct lrpc_invoke_req *invoke_reqs;

	pthread_mutex_t lock_poll;
	struct lrpc_packet pkt_buf;
};

struct lrpc_return_ctx
{
	struct lrpc_interface *inf;
	struct sockaddr_un addr;
	socklen_t addr_len;
	lrpc_cookie_t cookie;
};

int lrpc_endpoint_name(const struct lrpc_endpoint *endpoint, const char **name, size_t *name_len);

int lrpc_invoke_sync(struct lrpc_endpoint *endpoint,
                     const char *func_name, const void *args, size_t args_len,
                     void *ret_ptr, size_t *ret_size);

int lrpc_invoke(struct lrpc_endpoint *endpoint, struct lrpc_invoke_req *ctx);

/**
 * Get the arguments from context
 * @param ctx The context of function callback
 * @param pargs will be update to contain the address of arguments
 * @param args_len will be update to contain the size of arguments
 * @return -1 on failure, 0 on success
 */
int lrpc_get_args(const struct lrpc_invoke_ctx *ctx, void **pargs, size_t *args_len);

/**
 * Get the invoker from context
 * @param ctx The context of function callback
 * @param endpoint will be update to contain the endpoint of the invoker
 * @return -1 on failure, 0 on success
 */
int lrpc_get_invoker(const struct lrpc_invoke_ctx *ctx, struct lrpc_endpoint *endpoint);

/**
 * Get the credential information of the invoker
 * @param ctx The context of function callback
 * @param ucred will be update to contain the credential information of the invoker
 * @return -1 on failure, 0 on success
 */
int lrpc_get_ucred(const struct lrpc_invoke_ctx *ctx, struct lrpc_ucred *ucred);

void lrpc_init(struct lrpc_interface *inf, char *name, size_t name_len);

int lrpc_start(struct lrpc_interface *inf);

int lrpc_stop(struct lrpc_interface *inf);

/**
 * poll message on `inf`. If there is no message on `inf`, the current thread will block to wait a new message.
 *
 * @note Please avoid use `lrpc_poll` and `lrpc_try_poll` on same interface at same time. @see `lrpc_try_poll`.
 * @param inf
 * @return -1 on failure, 0 on success
 * @error EPROTO received an malformatted message
 * @error EINVAL received an invalid function invoke
 */
int lrpc_poll(struct lrpc_interface *inf);

/**
 * poll message on `inf` without blocking current thread.
 *
 * @note Please avoid use `lrpc_poll` and `lrpc_try_poll` on same interface at same time. It causes `lrpc_try_poll`
 * 		 fails with EBUSY. In this case, you must call lrpc_try_poll() again **after** lrpc_poll() finished, or your
 * 		 data may delayed or discard.
 * @param inf
 * @return -1 on failure, 0 on success
 * @error EBUSY Another thread is calling `lrpc_poll` or `lrpc_try_poll`.
 * @error EAGAIN There is no more messages to handle
 * @error EPROTO received an malformatted message
 * @error EINVAL received an invalid function invoke
 *
 */
int lrpc_try_poll(struct lrpc_interface *inf);

int lrpc_export_func(struct lrpc_interface *inf, struct lrpc_func *func);

int lrpc_connect(struct lrpc_interface *inf,
                 struct lrpc_endpoint *endpoint, const char *name, size_t name_len);

void lrpc_func_init(struct lrpc_func *func, const char *name, lrpc_func_cb callback, void *user_data);

int lrpc_return_async(const struct lrpc_invoke_ctx *ctx, struct lrpc_return_ctx *async_ctx);

int lrpc_return_finish(struct lrpc_return_ctx *ctx, const void *ret, size_t ret_size);

int lrpc_return(const struct lrpc_invoke_ctx *ctx, const void *ret, size_t ret_size);

int lrpc_msg_feed(struct lrpc_interface *inf, struct lrpc_packet *msg);

#endif //LRPC_LRPC_H
