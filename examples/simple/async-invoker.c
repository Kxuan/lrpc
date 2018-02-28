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
   <http://www.gnu.org/licenses/>.  */
#include <stdlib.h>
#include <string.h>
#include <lrpc.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include "common.h"

void callback(struct lrpc_invoke_req *ctx, int err_code, void *ret_ptr, size_t ret_size)
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
	struct lrpc_interface inf;
	struct lrpc_endpoint provider;
	struct lrpc_invoke_req invoke_req = {
		.func = "echo",
		.args = "hello",
		.args_size = sizeof("hello"),
		.cb = callback
	};
	pthread_t thd;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	rc = lrpc_start(&inf);
	assert(rc >= 0);

	rc = lrpc_connect(&inf, &provider, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	assert(rc >= 0);

/**
 * You can use "compound literals" with C99 or GCC
#if __STDC_VERSION__ >= 199901L || defined(__GNUC__)
	rc = lrpc_invoke_async(&provider, &(struct lrpc_invoke_req) {
		.func = "echo",
		.args = "hello",
		.args_size = sizeof("hello"),
		.cb = callback
	});
#endif
**/
	rc = lrpc_invoke(&provider, &invoke_req);
	assert(rc >= 0);

	rc = lrpc_poll(&inf);
	assert(rc >= 0);

	lrpc_stop(&inf);

	return 0;
}
