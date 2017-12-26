/*
 */

#include <utlist.h>
#include <string.h>
#include <malloc.h>

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

void lrpc_method_init(struct lrpc_method *method, const char *name, lrpc_method_cb callback, void *user_data)
{
	method->name = name;
	method->callback = callback;
	method->user_data = NULL;
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

void method_table_init(struct method_table *table)
{
	table->all_methods = NULL;
}
