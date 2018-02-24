/* Copyright (C) 2017-2018 kXuan <kxuanobj@gmail.com>

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
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <lrpc.h>
#include "test_common.h"

static void *thread_poll_routine(void *user_data)
{
	void *rc = NULL;
	struct lrpc_interface *inf = user_data;

	if (lrpc_poll(inf) < 0) {
		rc = (void *) (uintptr_t) errno;
	}
	return rc;
}

static int sync_rpc_echo(void *user_data, const struct lrpc_callback_ctx *ctx)
{
	void *args = NULL;
	size_t args_len = 0;
	struct lrpc_ucred cred;
	struct lrpc_endpoint ep;
	const char *name;
	size_t name_len;
	int rc;

	rc = lrpc_get_ucred(ctx, &cred);
	ck_assert_int_eq(rc, 0);
	ck_assert_int_ne(cred.pid, getpid());
	ck_assert_int_eq(cred.uid, getuid());
	ck_assert_int_eq(cred.gid, getgid());

	rc = lrpc_get_invoker(ctx, &ep);
	ck_assert_int_eq(rc, 0);
	rc = lrpc_endpoint_name(&ep, &name, &name_len);
	ck_assert_int_eq(rc, 0);
	ck_assert_int_eq(name_len, sizeof(NAME_INVOKER));
	ck_assert_int_eq(memcmp(name, NAME_INVOKER, sizeof(NAME_INVOKER)), 0);

	rc = lrpc_get_args(ctx, &args, &args_len);
	ck_assert_int_eq(rc, 0);
	ck_assert_str_eq(args, TEST_CONTENT);
	ck_assert_int_eq(args_len, sizeof(TEST_CONTENT));


	lrpc_return(ctx, args, args_len);

	return 0;
}

static int async_rpc(void *user_data, const struct lrpc_callback_ctx *ctx)
{
	void *args = NULL;
	size_t args_len = 0;

	lrpc_get_args(ctx, &args, &args_len);

	struct lrpc_return_ctx *async_ctx = user_data;
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
	struct lrpc_func func;
	ssize_t size;

	lrpc_init(&inf, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	lrpc_func_init(&func, TEST_METHOD, sync_rpc_echo, NULL);

	rc = lrpc_export_func(&inf, &func);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_start(&inf);
	ck_assert_int_ge(rc, 0);

	size = write(sigfd, "", 1);
	ck_assert_int_eq(size, 1);

	rc = lrpc_poll(&inf);
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
	size_t ret_size;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	lrpc_start(&inf);

	size = read(sigfd, buf, 1);
	ck_assert_int_eq(size, 1);

	rc = lrpc_connect(&inf, &peer, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	ck_assert_int_ge(rc, 0);

	rc = pthread_create(&thread, NULL, thread_poll_routine, &inf);
	ck_assert_int_ge(rc, 0);

	ret_size = sizeof(buf);
	rc = lrpc_call(&peer, TEST_METHOD, TEST_CONTENT, sizeof(TEST_CONTENT), buf, &ret_size);
	ck_assert_int_eq(rc, 0);
	ck_assert_int_eq(ret_size, sizeof(TEST_CONTENT));

	rc = pthread_join(thread, NULL);
	ck_assert_int_ge(rc, 0);

	ck_assert_str_eq(buf, TEST_CONTENT);
}

static void async_provider(int sigfd)
{
	int rc;
	ssize_t size;
	struct lrpc_interface inf;
	struct lrpc_func func;
	struct lrpc_return_ctx ctx = {0};

	lrpc_init(&inf, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	lrpc_func_init(&func, TEST_METHOD, async_rpc, &ctx);

	rc = lrpc_export_func(&inf, &func);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_start(&inf);
	ck_assert_int_ge(rc, 0);

	size = write(sigfd, "", 1);
	ck_assert_int_eq(size, 1);

	rc = lrpc_poll(&inf);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_return_finish(&ctx, TEST_CONTENT, sizeof(TEST_CONTENT));
	ck_assert_int_ge(rc, 0);
}

static void async_call_callback(struct lrpc_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
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
	struct lrpc_endpoint peer;
	ssize_t size;

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	lrpc_start(&inf);

	size = read(sigfd, buf, 1);
	ck_assert_int_eq(size, 1);

	rc = lrpc_connect(&inf, &peer, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	ck_assert_int_ge(rc, 0);

	struct lrpc_call_ctx ctx = {
		.func = TEST_METHOD,
		.args = TEST_CONTENT,
		.args_size = sizeof(TEST_CONTENT),
		.cb = async_call_callback
	};
	rc = lrpc_call_async(&peer, &ctx);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_poll(&inf);
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

int main()
{
	TCase *tc;

	int number_failed;
	Suite *s;
	SRunner *sr;

	s = suite_create(TEST_NAME);

	/* Core test case */
	tc = tcase_create("Core");
	tcase_set_timeout(tc, 0.1);
	tcase_add_test(tc, test_01sync);
	tcase_add_test(tc, test_02async_return);
	tcase_add_test(tc, test_03async_call);
	tcase_add_test(tc, test_04async_call_return);


	suite_add_tcase(s, tc);

	sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}