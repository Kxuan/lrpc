/*
 */

#ifndef LRPC_INTERFACE_H
#define LRPC_INTERFACE_H

#include "socket.h"
#include "method.h"

enum msgiov_types
{
	MSGIOV_HEAD,
	MSGIOV_BODY,
	MSGIOV_MAX
};

struct lrpc_interface
{
	struct lrpc_socket sock;
	struct method_table all_methods;
	//fixme
};

void lrpc_init(struct lrpc_interface *inf, char *name, size_t name_len);

int lrpc_start(struct lrpc_interface *inf);

int lrpc_poll(struct lrpc_interface *inf);

int lrpc_register_method(struct lrpc_interface *inf, struct lrpc_method *method);

int lrpc_connect(struct lrpc_interface *inf,
                 struct lrpc_endpoint *endpoint, const char *name, size_t name_len);

int lrpc_call(struct lrpc_endpoint *endpoint,
              const char *method_name, const void *args, size_t args_len,
              void *rc_ptr, size_t rc_size);

#endif //LRPC_INTERFACE_H
