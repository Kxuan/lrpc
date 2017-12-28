/*Copyright (C) 2017 kXuan <kxuanobj@gmail.com>

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
   <http://www.gnu.org/licenses/>.*/

#include <unistd.h>
#include <wait.h>
#include <stdlib.h>
#include <check.h>
#include <lrpc.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "../src/msg.h"
#include "../src/socket.h"
#include "test_common.h"

static void *thread_poll_routine(void *user_data)
{
	void *rc = NULL;
	struct lrpc_interface *inf = user_data;
	struct lrpc_packet rcv_buf;

	if (lrpc_poll(inf, &rcv_buf) < 0) {
		rc = (void *) (uintptr_t) errno;
	}
	return rc;
}

static int sync_rpc_echo(void *user_data, const struct lrpc_callback_ctx *ctx, void *args, size_t args_len)
{
	ck_assert_str_eq(args, TEST_CONTENT);
	ck_assert_int_eq(args_len, sizeof(TEST_CONTENT));

	lrpc_return(ctx, args, args_len);

	return 0;
}

static int async_rpc(void *user_data, const struct lrpc_callback_ctx *ctx, void *args, size_t args_len)
{
	struct lrpc_async_return_ctx *async_ctx = user_data;
	int rc;
	ck_assert_ptr_ne(user_data, NULL);
	ck_assert_str_eq(args, TEST_CONTENT);
	ck_assert_int_eq(args_len, sizeof(TEST_CONTENT));

	rc = lrpc_return_async(ctx, async_ctx);
	ck_assert_int_ge(rc, 0);
	return 0;
}

static void provider(int sigfd)
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_method method;
	struct lrpc_packet pkt_buffer;

	lrpc_init(&inf, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	lrpc_method_init(&method, TEST_METHOD, sync_rpc_echo, NULL);

	rc = lrpc_method(&inf, &method);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_start(&inf);
	ck_assert_int_ge(rc, 0);

	write(sigfd, "", 1);
	rc = lrpc_poll(&inf, &pkt_buffer);
	ck_assert_int_ge(rc, 0);

}

static void sync_invoker(int sigfd)
{
	int rc;
	char buf[sizeof(TEST_CONTENT)];
	struct lrpc_interface inf;
	struct lrpc_endpoint peer;
	pthread_t thread;
	ssize_t size;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	lrpc_start(&inf);

	size = read(sigfd, buf, 1);
	ck_assert_int_eq(size, 1);

	rc = lrpc_connect(&inf, &peer, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	ck_assert_int_ge(rc, 0);

	rc = pthread_create(&thread, NULL, thread_poll_routine, &inf);
	ck_assert_int_ge(rc, 0);

	rc = (int) lrpc_call(&peer, TEST_METHOD, TEST_CONTENT, sizeof(TEST_CONTENT), buf, sizeof(buf));
	ck_assert_int_ge(rc, 0);

	rc = pthread_join(thread, NULL);
	ck_assert_int_ge(rc, 0);

	ck_assert_str_eq(buf, TEST_CONTENT);
}

static void async_provider(int sigfd)
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_method method;
	struct lrpc_packet pkt_buffer;
	struct lrpc_async_return_ctx ctx = {0};

	lrpc_init(&inf, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	lrpc_method_init(&method, TEST_METHOD, async_rpc, &ctx);

	rc = lrpc_method(&inf, &method);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_start(&inf);
	ck_assert_int_ge(rc, 0);

	write(sigfd, "", 1);
	rc = lrpc_poll(&inf, &pkt_buffer);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_return_finish(&ctx, TEST_CONTENT, sizeof(TEST_CONTENT));
	ck_assert_int_ge(rc, 0);
}

static void async_call_callback(struct lrpc_async_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
{
	ck_assert_int_eq(err_code, 0);
	ck_assert_int_eq(ret_size, sizeof(TEST_CONTENT));
	ck_assert_str_eq(ret_ptr, TEST_CONTENT);
	ctx->user_data = (void *) 1;
}

static void async_invoker(int sigfd)
{
	int rc;
	char buf[sizeof(TEST_CONTENT)];
	struct lrpc_interface inf;
	struct lrpc_packet pkt_buffer;
	struct lrpc_endpoint peer;
	ssize_t size;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	lrpc_start(&inf);

	size = read(sigfd, buf, 1);
	ck_assert_int_eq(size, 1);

	rc = lrpc_connect(&inf, &peer, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	ck_assert_int_ge(rc, 0);

	struct lrpc_async_call_ctx ctx;
	rc = lrpc_call_async(&peer, &ctx, TEST_METHOD, TEST_CONTENT, sizeof(TEST_CONTENT), async_call_callback);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_poll(&inf, &pkt_buffer);
	ck_assert_int_ge(rc, 0);

	ck_assert_ptr_eq(ctx.user_data, (void *) 1);

}

START_TEST(test_01sync)
	int rc;
	pid_t pid;
	int fds[2];

	rc = pipe(fds);

	ck_assert_int_ge(rc, 0);

	pid = fork();
	ck_assert_int_ge(pid, 0);

	if (pid == 0) {
		provider(fds[1]);
		exit(0);
	} else {
		sync_invoker(fds[0]);
		rc = waitpid(pid, NULL, 0);
		ck_assert_int_eq(rc, pid);
	}
END_TEST

START_TEST(test_02async_return)
	int rc;
	pid_t pid;
	int fds[2];

	rc = pipe(fds);

	ck_assert_int_ge(rc, 0);

	pid = fork();
	ck_assert_int_ge(pid, 0);

	if (pid == 0) {
		async_provider(fds[1]);
		exit(0);
	} else {
		sync_invoker(fds[0]);
		rc = waitpid(pid, NULL, 0);
		ck_assert_int_eq(rc, pid);
	}
END_TEST

START_TEST(test_03async_call)
	int rc;
	pid_t pid;
	int fds[2];

	rc = pipe(fds);

	ck_assert_int_ge(rc, 0);

	pid = fork();
	ck_assert_int_ge(pid, 0);

	if (pid == 0) {
		provider(fds[1]);
		exit(0);
	} else {
		async_invoker(fds[0]);
		rc = waitpid(pid, NULL, 0);
		ck_assert_int_eq(rc, pid);
	}
END_TEST

START_TEST(test_04async_call_return)
	int rc;
	pid_t pid;
	int fds[2];

	rc = pipe(fds);

	ck_assert_int_ge(rc, 0);

	pid = fork();
	ck_assert_int_ge(pid, 0);

	if (pid == 0) {
		async_provider(fds[1]);
		exit(0);
	} else {
		async_invoker(fds[0]);
		rc = waitpid(pid, NULL, 0);
		ck_assert_int_eq(rc, pid);
	}
END_TEST

TCase *create_tcase_core()
{
	TCase *tc;

	/* Core test case */
	tc = tcase_create("Core");
	tcase_set_timeout(tc, 0.1);
	tcase_add_test(tc, test_01sync);
	tcase_add_test(tc, test_02async_return);
	tcase_add_test(tc, test_03async_call);
	tcase_add_test(tc, test_04async_call_return);

	return tc;
}