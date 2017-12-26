/*
 */

#ifndef LRPC_SOCKET_H
#define LRPC_SOCKET_H

#include <stdint.h>
#include <limits.h>
#include <sys/socket.h>
#include <linux/un.h>
#include "method.h"

#define LRPC_MSG_NAME_MAX (UNIX_PATH_MAX - (sizeof(LRPC_MSG_NAME_PREFIX) - 1))
#define LRPC_MAX_PACKET_SIZE (512)

struct lrpc_socket;

struct lrpc_endpoint
{
	struct lrpc_socket *sock;
	socklen_t addr_len;
	struct sockaddr_un addr;
};

struct lrpc_socket
{
	int fd;
	size_t recv_buf_size, send_buf_size;
	char recv_buf[LRPC_MAX_PACKET_SIZE];
	char send_buf[LRPC_MAX_PACKET_SIZE];
	struct lrpc_endpoint endpoint;
};

enum lrpc_msg_types
{
	LRPC_MSGTYP_CALL,
	LRPC_MSGTYP_RETURN_VAL,
	LRPC_MSGTYP_RETURN_ERROR
};

struct lrpc_msg_head
{
	uintptr_t cookie;
	uint16_t body_size;
	uint8_t type; //enum lrpc_msg_types
};

struct lrpc_msg_call
{
	struct lrpc_msg_head head;
	uint8_t method_len;
	uint16_t args_len;
	char method[LRPC_METHOD_NAME_MAX];
};

struct lrpc_msg_return
{
	struct lrpc_msg_head head;
	uint16_t returns_len;
};

void endpoint_init(struct lrpc_endpoint *endpoint, const char *name, size_t name_len);

int endpoint_open(struct lrpc_socket *sock, struct lrpc_endpoint *endpoint, const char *name, size_t name_len);

ssize_t lrpc_socket_recvmsg(struct lrpc_socket *sock, struct msghdr *hdr, int flags);

void lrpc_socket_init(struct lrpc_socket *sock, const char *name, size_t name_len);

int lrpc_socket_open(struct lrpc_socket *sock);

#endif //LRPC_SOCKET_H