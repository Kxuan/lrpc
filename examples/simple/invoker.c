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
#include "common.h"

int main()
{
	int rc;
	char buf[10];
	struct lrpc_interface inf;
	struct lrpc_endpoint peer;
	ssize_t size;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	lrpc_start(&inf);

	rc = lrpc_connect(&inf, &peer, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	if (rc < 0) {
		perror("lrpc_connect");
		abort();
	}

	rc = lrpc_call(&peer, "echo", "hello", 6, buf, 10);
	if (rc < 0) {
		perror("lrpc_call");
		abort();
	}

	if (strcmp(buf, "hello") != 0) {
		fprintf(stderr, "result mismatched!\n");
		return 1;
	}
	return 0;
}
