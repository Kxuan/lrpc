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


#include <sys/un.h>
#include <utlist.h>
#include <lrpc.h>
#include <stdio.h>
#include <pthread.h>
#include <lrpc-internal.h>
#include <unistd.h>
#include <errno.h>
#include "interface.h"
#include "msg.h"
#include "endpoint.h"

int inf_async_call(struct lrpc_interface *inf, struct lrpc_async_call_ctx *ctx, struct msghdr *msg)
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

EXPORT int lrpc_method(struct lrpc_interface *inf, struct lrpc_method *method)
{
	return method_register(&inf->all_methods, method);
}

EXPORT void lrpc_init(struct lrpc_interface *inf, char *name, size_t name_len)
{
	pthread_mutex_init(&inf->lock_call_list, NULL);
	pthread_mutex_init(&inf->lock_recv, NULL);

	inf->call_list = NULL;
	inf->fd = -1;
	endpoint_init(&inf->local_endpoint, inf, name, name_len);
	method_table_init(&inf->all_methods);
}

EXPORT int lrpc_start(struct lrpc_interface *inf)
{
	int fd;
	int rc;

	assert(inf->fd < 0);

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		goto err;
	}

	rc = bind(fd, (struct sockaddr *) &inf->local_endpoint.addr, inf->local_endpoint.addr_len);
	if (rc < 0) {
		goto err_bind;
	}
	inf->fd = fd;
	return 0;
err_bind:
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

int lrpc_poll_unsafe(struct lrpc_interface *inf, struct lrpc_packet *pkt, size_t payload_size)
{
	int rc;
	ssize_t size;
	
	pkt->iov.iov_base = pkt->payload;
	pkt->iov.iov_len = payload_size;

	pkt->msgh.msg_name = &pkt->addr;
	pkt->msgh.msg_namelen = sizeof(pkt->addr);
	pkt->msgh.msg_controllen = 0;
	pkt->msgh.msg_control = NULL;
	pkt->msgh.msg_iov = &pkt->iov;
	pkt->msgh.msg_iovlen = 1;

	size = recvmsg(inf->fd, &pkt->msgh, 0);
	if (size <= 0) {
		goto err;
	}
	pkt->payload_len = (size_t) size;

	rc = lrpc_msg_feed(inf, pkt);
	return rc;

err:
	return -1;
}

EXPORT int lrpc_poll(struct lrpc_interface *inf)
{
	int rc;

	rc = pthread_mutex_trylock(&inf->lock_recv);
	if (rc != 0) {
		errno = rc;
		return -1;
	}

	rc = lrpc_poll_unsafe(inf, &inf->packet_recv, sizeof(inf->packet_recv.payload));

	pthread_mutex_unlock(&inf->lock_recv);

	return rc;
}
