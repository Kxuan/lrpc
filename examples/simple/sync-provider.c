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
#include "common.h"

int sync_rpc_echo(void *user_data,
                  const struct lrpc_callback_ctx *ctx)
{
	void *args;
	size_t args_len;
	struct lrpc_ucred cred;

	lrpc_get_args(ctx, &args, &args_len);
	if (lrpc_get_ucred(ctx, &cred) == 0) {
		fprintf(stderr, "%s: invoked by pid = %d, uid = %d, gid = %d\n", __func__, cred.pid, cred.uid, cred.gid);
	}

	lrpc_return(ctx, args, args_len);

	return 0;
}

int main()
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_func func;

	lrpc_init(&inf, NAME_PROVIDER, sizeof(NAME_PROVIDER));

	lrpc_func_init(&func, "echo", sync_rpc_echo, NULL);
	rc = lrpc_export_func(&inf, &func);
	if (rc < 0) {
		perror("lrpc_export_func");
		abort();
	}

	rc = lrpc_start(&inf);
	if (rc < 0) {
		perror("lrpc_start");
		abort();
	}

	rc = lrpc_poll(&inf);
	if (rc < 0) {
		perror("lrpc_poll");
		abort();
	}

	lrpc_stop(&inf);

	return 0;
}