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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <interface.h>
#include <stdio.h>
#include <assert.h>
#include "common.h"

int async_call_callback(struct lrpc_async_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
{
	ctx->user_data = (void *) 1;
}

int main()
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_packet pkt_buffer;
	struct lrpc_endpoint peer;
	ssize_t size;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	lrpc_start(&inf);

	rc = lrpc_connect(&inf, &peer, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	assert(rc >= 0);

	struct lrpc_async_call_ctx ctx;
	ctx.user_data = NULL;
	ctx.method = "echo";
	ctx.args = "hello";
	ctx.args_len = 6;
	ctx.cb = async_call_callback;

	rc = lrpc_call_async(&peer, &ctx);
	assert(rc >= 0);

	rc = lrpc_poll(&inf, &pkt_buffer);
	assert(rc >= 0);

	assert(ctx.user_data == (void *) 1);

	lrpc_stop(&inf);

	return 0;
}
