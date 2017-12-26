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


#ifndef LRPC_SOCKET_H
#define LRPC_SOCKET_H

#include <stdint.h>
#include <limits.h>
#include <sys/socket.h>
#include <linux/un.h>

struct lrpc_socket;
struct lrpc_packet;
struct lrpc_msg_call;

#define LRPC_MAX_PACKET_SIZE (512)

struct lrpc_endpoint
{
	struct lrpc_socket *sock;
	socklen_t addr_len;
	struct sockaddr_un addr;
};

struct lrpc_socket
{
	int fd;
	struct lrpc_endpoint endpoint;
};

#include "msg.h"

struct lrpc_packet
{
	char context[sizeof(union lrpc_msg_ctx)];
	struct sockaddr_un addr;
	struct msghdr msgh;
	struct iovec iov;
	size_t payload_len;
	char payload[];
};


void endpoint_init(struct lrpc_endpoint *endpoint, struct lrpc_socket *sock, const char *name, size_t name_len);

void lrpc_socket_freemsg(struct lrpc_packet *m);

struct lrpc_packet *lrpc_socket_recvmsg(struct lrpc_socket *sock, int flags);

void lrpc_socket_init(struct lrpc_socket *sock, const char *name, size_t name_len);

int lrpc_socket_open(struct lrpc_socket *sock);

void lrpc_socket_close(struct lrpc_socket *sock);

#endif //LRPC_SOCKET_H
