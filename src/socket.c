/*
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "socket.h"

void endpoint_init(struct lrpc_endpoint *endpoint, const char *name, size_t name_len)
{
	char *p;

	endpoint->addr.sun_family = AF_UNIX;
	p = endpoint->addr.sun_path;
	*p++ = 0;
	memcpy(p, LRPC_MSG_NAME_PREFIX, sizeof(LRPC_MSG_NAME_PREFIX) - 1);
	p += sizeof(LRPC_MSG_NAME_PREFIX) - 1;
	strcpy(p, name);
	p += name_len;
	endpoint->addr_len = (socklen_t) (p - endpoint->addr.sun_path);
}

void lrpc_socket_init(struct lrpc_socket *sock, const char *name, size_t name_len)
{
	struct lrpc_endpoint *endpoint = &sock->endpoint;

	endpoint->sock = sock;
	endpoint_init(endpoint, name, name_len);
	sock->recv_buf_size = sizeof(sock->recv_buf);
	sock->send_buf_size = sizeof(sock->send_buf);
	sock->fd = -1;
}

int lrpc_socket_open(struct lrpc_socket *sock)
{
	int fd;
	int rc;

	assert(sock->fd < 0);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		goto err;
	}

	rc = bind(fd, (struct sockaddr *) &sock->endpoint.addr, sock->endpoint.addr_len);
	if (rc < 0) {
		goto err_bind;
	}

	sock->fd = fd;
	return 0;
err_bind:
	close(fd);
err:
	return -1;
}

void lrpc_socket_close(struct lrpc_socket *sock)
{
	if (sock->fd >= 0) {
		close(sock->fd);
		sock->fd = -1;
	}
}

int endpoint_open(struct lrpc_socket *sock, struct lrpc_endpoint *endpoint, const char *name, size_t name_len)
{
	assert(sock && endpoint && name);

	endpoint_init(endpoint, name, name_len);
	endpoint->sock = sock;
	return 0;
}

void lrpc_endpoint_close(struct lrpc_socket *sock, struct lrpc_endpoint *endpoint)
{

}

ssize_t lrpc_socket_recvmsg(struct lrpc_socket *sock, struct msghdr *m, int flags)
{
	m->msg_controllen = 0;
	m->msg_control = NULL;
	m->msg_iov->iov_base = sock->recv_buf;
	m->msg_iov->iov_len = sock->recv_buf_size;
	m->msg_iovlen = 1;
	return recvmsg(sock->fd, m, flags);
}

ssize_t lrpc_socket_sendmsg(struct lrpc_socket *sock, struct msghdr *m, int flags)
{
	return sendmsg(sock->fd, m, flags);
}
