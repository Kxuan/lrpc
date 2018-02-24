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

struct rpc_context
{
	struct lrpc_return_ctx return_ctx;
	char buf[10];
	size_t size;
};

int async_rpc_echo(void *user_data, const struct lrpc_callback_ctx *ctx)
{
	void *args;
	size_t args_len;
	struct rpc_context *p = user_data;
	struct lrpc_ucred cred;
	struct lrpc_endpoint ep;
	size_t copy_size;
	const char *name;
	size_t name_len;

	lrpc_get_args(ctx, &args, &args_len);

	if (lrpc_get_ucred(ctx, &cred) < 0) {
		perror("lrpc_get_ucred");
	}
	if (lrpc_get_invoker(ctx, &ep) < 0) {
		perror("lrpc_get_invoker");
	}
	if (lrpc_endpoint_name(&ep, &name, &name_len) < 0) {
		perror("lrpc_endpoint_name");
	}
	fprintf(stderr, "%s: invoked by \"%-*s\" with pid = %d, uid = %d, gid = %d\n", __func__,
	        (int) name_len, name,
	        cred.pid, cred.uid, cred.gid);

	copy_size = sizeof(p->buf) > args_len ? args_len : sizeof(p->buf);
	memcpy(p->buf, args, copy_size);
	p->size = copy_size;

	lrpc_return_async(ctx, &p->return_ctx);

	return 0;
}

int main()
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_func func;
	struct rpc_context ctx;

	lrpc_init(&inf, NAME_PROVIDER, sizeof(NAME_PROVIDER));

	lrpc_func_init(&func, "echo", async_rpc_echo, &ctx);
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
	lrpc_return_finish(&ctx.return_ctx, ctx.buf, ctx.size);

	lrpc_stop(&inf);

	return 0;
}