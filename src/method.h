/*
 */

#ifndef LRPC_METHOD_H
#define LRPC_METHOD_H

#include "lrpc.h"

#define LRPC_METHOD_NAME_MAX (32)

typedef enum lrpc_return_status
{
	LRPC_RETURN_VAL,
	LRPC_RETURN_ERROR
} lrpc_return_status;


typedef lrpc_return_status (*lrpc_method_cb)(void *user_data,
                                             void *args, size_t args_len,
                                             void *ret_val, size_t *ret_size);

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
