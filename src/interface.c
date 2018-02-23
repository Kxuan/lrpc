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
#include <sys/un.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <utlist.h>
#include <lrpc.h>
#include <lrpc-internal.h>
#include "interface.h"
#include "msg.h"
#include "endpoint.h"

int inf_async_call(struct lrpc_interface *inf, struct lrpc_call_ctx *ctx, struct msghdr *msg)
{
	ssize_t size;
	pthread_mutex_lock(&inf->lock_call_list);
	DL_APPEND(inf->call_list, ctx);
	pthread_mutex_unlock(&inf->lock_call_list);
	size = sendmsg(inf->fd, msg, 0);
	if (size <= 0) {
		goto err;
	}
	return 0;
err:
	pthread_mutex_lock(&inf->lock_call_list);
	DL_DELETE(inf->call_list, ctx);
	pthread_mutex_unlock(&inf->lock_call_list);
	return -1;
}

EXPORT int lrpc_connect(struct lrpc_interface *inf,
                        struct lrpc_endpoint *endpoint, const char *name, size_t name_len)
{
	endpoint_init(endpoint, inf, name, name_len);
	return 0;
}

EXPORT int lrpc_export_func(struct lrpc_interface *inf, struct lrpc_func *func)
{
	return func_add(&inf->all_funcs, func);
}

EXPORT void lrpc_init(struct lrpc_interface *inf, char *name, size_t name_len)
{
	pthread_mutex_init(&inf->lock_call_list, NULL);
	pthread_mutex_init(&inf->lock_poll, NULL);

	inf->call_list = NULL;
	inf->fd = -1;
	inf->pkt_buf.payload_size = sizeof(inf->pkt_buf.payload);
	endpoint_init(&inf->local_endpoint, inf, name, name_len);
	func_table_init(&inf->all_funcs);
}

EXPORT int lrpc_start(struct lrpc_interface *inf)
{
	int fd;
	int rc;
	int v;

	assert(inf->fd < 0);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		goto err;
	}

	rc = bind(fd, (struct sockaddr *) &inf->local_endpoint.addr, inf->local_endpoint.addr_len);
	if (rc < 0) {
		goto err_close;
	}

	v = 1;
	rc = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &v, sizeof(v));
	if (rc < 0) {
		goto err_close;
	}

	inf->fd = fd;
	return 0;
err_close:
	close(fd);
err:
	return -1;
}

EXPORT int lrpc_stop(struct lrpc_interface *inf)
{
	if (inf->fd >= 0) {
		close(inf->fd);
		inf->fd = -1;
	}
}

int inf_poll_unsafe(struct lrpc_interface *inf, struct lrpc_packet *buf, int blocking)
{
	int rc;
	int flags = blocking ? 0 : MSG_DONTWAIT;
	struct lrpc_packet *p = buf;
	ssize_t size;

	p->iov.iov_base = p->payload;
	p->iov.iov_len = p->payload_size;

	p->msgh.msg_name = &p->addr;
	p->msgh.msg_namelen = sizeof(p->addr);
	p->msgh.msg_controllen = sizeof(p->cmsg);
	p->msgh.msg_control = p->cmsg;
	p->msgh.msg_iov = &p->iov;
	p->msgh.msg_iovlen = 1;

	size = recvmsg(inf->fd, &p->msgh, flags);
	if (size <= 0) {
		goto err;
	}
	p->payload_len = (size_t) size;

	rc = lrpc_msg_feed(inf, p);
	return rc;

err:
	return -1;
}

EXPORT int lrpc_try_poll(struct lrpc_interface *inf)
{
	int rc;
	struct lrpc_packet *pkt = &inf->pkt_buf;

	rc = pthread_mutex_trylock(&inf->lock_poll);
	if (rc != 0) {
		errno = rc;
		return -1;
	}

	rc = inf_poll_unsafe(inf, pkt, 0);

	pthread_mutex_unlock(&inf->lock_poll);

	return rc;
}

EXPORT int lrpc_poll(struct lrpc_interface *inf)
{
	int rc;
	struct lrpc_packet *pkt = &inf->pkt_buf;

	pthread_mutex_lock(&inf->lock_poll);

	rc = inf_poll_unsafe(inf, pkt, 1);

	pthread_mutex_unlock(&inf->lock_poll);

	return rc;
}
