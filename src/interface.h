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


#ifndef LRPC_INTERFACE_H
#define LRPC_INTERFACE_H

#include "socket.h"
#include "method.h"

void lrpc_init(struct lrpc_interface *inf, char *name, size_t name_len);

int lrpc_start(struct lrpc_interface *inf);

int lrpc_stop(struct lrpc_interface *inf);

int lrpc_poll(struct lrpc_interface *inf, struct lrpc_packet *buffer);

int lrpc_method(struct lrpc_interface *inf, struct lrpc_method *method);

int lrpc_connect(struct lrpc_interface *inf,
                 struct lrpc_endpoint *endpoint, const char *name, size_t name_len);

int inf_async_call(struct lrpc_interface *inf, struct lrpc_async_call_ctx *ctx, struct msghdr *msg);

#endif //LRPC_INTERFACE_H
