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

#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "socket.h"

void endpoint_init(struct lrpc_endpoint *endpoint, struct lrpc_socket *sock, const char *name, size_t name_len)
{
	char *p;

	assert(endpoint && name_len && name);

	endpoint->sock = sock;
	endpoint->addr.sun_family = AF_UNIX;
	p = endpoint->addr.sun_path;
	*p++ = 0;
	memcpy(p, LRPC_MSG_NAME_PREFIX, sizeof(LRPC_MSG_NAME_PREFIX) - 1);
	p += sizeof(LRPC_MSG_NAME_PREFIX) - 1;
	memcpy(p, name, name_len);
	p += name_len;
	endpoint->addr_len = (socklen_t) (offsetof(struct sockaddr_un, sun_path) + (p - endpoint->addr.sun_path));
}

void lrpc_socket_init(struct lrpc_socket *sock, const char *name, size_t name_len)
{
	struct lrpc_endpoint *endpoint = &sock->endpoint;

	endpoint->sock = sock;
	endpoint_init(endpoint, NULL, name, name_len);
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

void lrpc_endpoint_close(struct lrpc_socket *sock, struct lrpc_endpoint *endpoint)
{

}

void lrpc_socket_freemsg(struct lrpc_packet *m)
{
	free(m);
}

struct lrpc_packet *lrpc_socket_recvmsg(struct lrpc_socket *sock, int flags)
{
	ssize_t size;
	struct lrpc_packet *m;

	m = (struct lrpc_packet *) malloc(sizeof(struct lrpc_packet) + LRPC_MAX_PACKET_SIZE);
	if (!m) {
		goto err;
	}

	m->iov.iov_base = m->payload;
	m->iov.iov_len = LRPC_MAX_PACKET_SIZE;

	m->msgh.msg_name = &m->addr;
	m->msgh.msg_namelen = sizeof(m->addr);
	m->msgh.msg_controllen = 0;
	m->msgh.msg_control = NULL;
	m->msgh.msg_iov = &m->iov;
	m->msgh.msg_iovlen = 1;

	size = recvmsg(sock->fd, &m->msgh, flags);
	if (size <= 0) {
		goto err_recvmsg;
	}
	m->payload_len = (size_t) size;
	return m;

err_recvmsg:
	free(m);
err:
	return NULL;
}

ssize_t lrpc_socket_sendmsg(struct lrpc_socket *sock, struct msghdr *m, int flags)
{
	return sendmsg(sock->fd, m, flags);
}
