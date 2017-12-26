#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <interface.h>
#include <stdio.h>
#include "common.h"


lrpc_return_status provider_echo(void *user_data,
                                 void *args, size_t args_len,
                                 void *ret_val, size_t *ret_size)
{
	memcpy(ret_val, args, args_len);
	*ret_size = args_len;

	return LRPC_RETURN_VAL;
}

int main()
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_method method;

	lrpc_init(&inf, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	lrpc_method_init(&method, "echo", provider_echo, NULL);

	rc = lrpc_register_method(&inf, &method);
	if (rc < 0) {
		perror("lrpc_register_method");
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

	return 0;
}