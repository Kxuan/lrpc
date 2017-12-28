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
#include <stdlib.h>
#include <string.h>
#include <interface.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include "common.h"

void callback(struct lrpc_async_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
{
	if (strncmp(ret_ptr, "hello", ret_size) != 0) {
		fprintf(stderr, "result mismatched!\n");
		abort();
	}
	printf("got result: %*s\n", (int) ret_size, (char *) ret_ptr);
}

int main()
{
	int rc;
	struct lrpc_packet pkt_buf;
	struct lrpc_interface inf;
	struct lrpc_endpoint provider;
	struct lrpc_async_call_ctx call_ctx;
	pthread_t thd;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	rc = lrpc_start(&inf);
	assert(rc >= 0);

	rc = lrpc_connect(&inf, &provider, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	assert(rc >= 0);

	rc = lrpc_call_async(&provider, &call_ctx, "echo", "hello", 6, callback);
	assert(rc >= 0);

	rc = lrpc_poll(&inf, &pkt_buf);
	assert(rc >= 0);

	lrpc_stop(&inf);

	return 0;
}
