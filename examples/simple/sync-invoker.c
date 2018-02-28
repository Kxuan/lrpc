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

void *thread_invoke(void *data)
{
	int rc;
	size_t ret_size;
	char buf[10];
	struct lrpc_endpoint *provider = data;

	ret_size = sizeof(buf);
	rc = lrpc_invoke_sync(provider, "echo", "hello", 6, buf, &ret_size);
	if (rc < 0) {
		perror("lrpc_invoke_sync");
		abort();
	}
	if (ret_size >= sizeof(buf)) {
		fprintf(stderr, "illegal size\n");
		abort();
	}
	if (strncmp(buf, "hello", ret_size) != 0) {
		fprintf(stderr, "result mismatched!\n");
		abort();
	}
	printf("got result: %*s\n", (int) ret_size, buf);
}

int main()
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_endpoint provider;
	pthread_t thd;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	rc = lrpc_start(&inf);
	assert(rc >= 0);

	rc = lrpc_connect(&inf, &provider, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	assert(rc >= 0);

	rc = pthread_create(&thd, NULL, thread_invoke, &provider);
	assert(rc >= 0);

	rc = lrpc_poll(&inf);
	assert(rc >= 0);

	rc = pthread_join(thd, NULL);
	assert(rc >= 0);

	lrpc_stop(&inf);

	return 0;
}
