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


#include <utlist.h>
#include <string.h>
#include <malloc.h>
#include <lrpc-internal.h>

#include "method.h"

struct lrpc_method *method_find(const struct method_table *table, const char *method_name, size_t method_len)
{
	struct lrpc_method *method;

	if (method_len > LRPC_METHOD_NAME_MAX)
		method_len = LRPC_METHOD_NAME_MAX;

	DL_FOREACH(table->all_methods, method) {
		// todo compare the '\0'
		if (strncmp(method_name, method->name, method_len) == 0) {
			return method;
		}
	}
}

int method_register(struct method_table *table, struct lrpc_method *method)
{
	DL_APPEND(table->all_methods, method);
	return 0;
}

void method_deregister(struct method_table *table, struct lrpc_method *method)
{
	if (!method) {
		return;
	}

	DL_DELETE(table->all_methods, method);
}

EXPORT void lrpc_method_init(struct lrpc_method *method, const char *name, lrpc_method_cb callback, void *user_data)
{
	method->name = name;
	method->callback = callback;
	method->user_data = user_data;
}

struct lrpc_method *lrpc_method_create(const char *name, lrpc_method_cb callback, void *user_data)
{
	struct lrpc_method *method = (struct lrpc_method *) malloc(sizeof(struct lrpc_method));
	if (!method) {
		return NULL;
	}
	lrpc_method_init(method, name, callback, user_data);
	return method;
}

void lrpc_method_destroy(struct lrpc_method *method)
{
	free(method);
}

void method_table_init(struct method_table *table)
{
	table->all_methods = NULL;
}
