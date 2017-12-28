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
#include <sys/un.h>
#include <lrpc.h>

#include "endpoint.h"
#include "interface.h"
#include "msg.h"

int socket_recvmsg(struct lrpc_socket *sock, struct lrpc_packet *pkt, int flags);

ssize_t socket_sendmsg(struct lrpc_socket *sock, struct msghdr *m, int flags);

void lrpc_socket_init(struct lrpc_socket *sock, struct lrpc_interface *inf, const char *name, size_t name_len);

int lrpc_socket_open(struct lrpc_socket *sock);

void lrpc_socket_close(struct lrpc_socket *sock);

#endif //LRPC_SOCKET_H
