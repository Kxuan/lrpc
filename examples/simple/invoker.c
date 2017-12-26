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
