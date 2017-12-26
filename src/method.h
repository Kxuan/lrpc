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


#ifndef LRPC_METHOD_H
#define LRPC_METHOD_H

#define LRPC_METHOD_NAME_MAX (32)

typedef int (*lrpc_method_cb)(void *user_data, const void *call, void *args, size_t args_len);

struct lrpc_method
{
	const char *name;
	lrpc_method_cb callback;
	void *user_data;

	struct lrpc_method *prev, *next;
};

struct method_table
{
	struct lrpc_method *all_methods;
};

void method_table_init(struct method_table *table);

struct lrpc_method *method_find(const struct method_table *table, const char *method_name, size_t method_len);

int method_register(struct method_table *table, struct lrpc_method *method);

void method_deregister(struct method_table *table, struct lrpc_method *method);

void lrpc_method_init(struct lrpc_method *method, const char *name, lrpc_method_cb callback, void *user_data);

struct lrpc_method *lrpc_method_create(const char *name, lrpc_method_cb callback, void *user_data);

#endif //LRPC_METHOD_H
