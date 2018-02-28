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


#include <utlist.h>
#include <string.h>
#include <lrpc-internal.h>

#include "func_table.h"

struct lrpc_func *func_find(const struct func_table *table, const char *func_name, size_t func_len)
{
	struct lrpc_func *func;

	if (func_len > LRPC_FUNC_NAME_MAX)
		func_len = LRPC_FUNC_NAME_MAX;

	DL_FOREACH(table->all_funcs, func) {
		// todo compare the '\0'
		if (strncmp(func_name, func->name, func_len) == 0) {
			return func;
		}
	}
}

int func_add(struct func_table *table, struct lrpc_func *func)
{
	DL_APPEND(table->all_funcs, func);
	return 0;
}

void func_remove(struct func_table *table, struct lrpc_func *func)
{
	if (!func) {
		return;
	}

	DL_DELETE(table->all_funcs, func);
}

EXPORT void lrpc_func_init(struct lrpc_func *func, const char *name, lrpc_func_cb callback, void *user_data)
{
	func->name = name;
	func->callback = callback;
	func->user_data = user_data;
}

void func_table_init(struct func_table *table)
{
	table->all_funcs = NULL;
}
