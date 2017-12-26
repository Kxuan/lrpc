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
#include <check.h>
#include <lrpc.h>
#include <interface.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

char name_provider[] = "provider";
char name_invoker[] = "invoker";

int sync_rpc_echo(void *user_data,
                  const void *ctx,
                  void *args, size_t args_len)
{
	lrpc_return(ctx, args, args_len);

	return 0;
}

void sync_provider(int sigfd)
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_method method;

	lrpc_init(&inf, name_provider, sizeof(name_provider));
	lrpc_method_init(&method, "echo", sync_rpc_echo, NULL);

	rc = lrpc_register_method(&inf, &method);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_start(&inf);
	ck_assert_int_ge(rc, 0);

	write(sigfd, "", 1);
	rc = lrpc_poll(&inf);
	ck_assert_int_ge(rc, 0);

}

void sync_invoker(int sigfd)
{
	int rc;
	char buf[10];
	struct lrpc_interface inf;
	struct lrpc_endpoint peer;
	ssize_t size;

	lrpc_init(&inf, name_invoker, sizeof(name_invoker));

	lrpc_start(&inf);

	size = read(sigfd, buf, 1);
	ck_assert_int_eq(size, 1);

	rc = lrpc_connect(&inf, &peer, name_provider, sizeof(name_provider));
	ck_assert_int_ge(rc, 0);

	rc = lrpc_call(&peer, "echo", "hello", 6, buf, 10);
	ck_assert_int_ge(rc, 0);

	ck_assert_str_eq(buf, "hello");
}

START_TEST(test_sync_return)
	int rc;
	pid_t pid;
	int fds[2];

	rc = pipe(fds);

	ck_assert_int_ge(rc, 0);

	pid = fork();
	ck_assert_int_ge(pid, 0);

	if (pid == 0) {
		sync_provider(fds[1]);
		exit(0);
	} else {
		sync_invoker(fds[0]);
		rc = waitpid(pid, NULL, 0);
		ck_assert_int_eq(rc, pid);
	}
END_TEST

int async_rpc(void *user_data,
              const void *ctx,
              void *args, size_t args_len)
{
	int rc;
	(*(const void **) user_data) = ctx;
	rc = lrpc_return_async(ctx);
	ck_assert_int_ge(rc, 0);
	return 0;
}

void async_provider(int sigfd)
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_method method;
	const char *async_ctx = NULL;

	lrpc_init(&inf, name_provider, sizeof(name_provider));
	lrpc_method_init(&method, "async", async_rpc, &async_ctx);

	rc = lrpc_register_method(&inf, &method);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_start(&inf);
	ck_assert_int_ge(rc, 0);

	write(sigfd, "", 1);
	rc = lrpc_poll(&inf);
	ck_assert_int_ge(rc, 0);

	ck_assert_ptr_ne(async_ctx, NULL);
	rc = lrpc_return(async_ctx, "async_test", 10);
	ck_assert_int_ge(rc, 0);
}

void async_invoker(int sigfd)
{
	int rc;
	char buf[10];
	struct lrpc_interface inf;
	struct lrpc_endpoint peer;
	ssize_t size;

	lrpc_init(&inf, name_invoker, sizeof(name_invoker));

	lrpc_start(&inf);

	size = read(sigfd, buf, 1);
	ck_assert_int_eq(size, 1);

	rc = lrpc_connect(&inf, &peer, name_provider, sizeof(name_provider));
	ck_assert_int_ge(rc, 0);

	rc = lrpc_call(&peer, "async", "hello", 6, buf, 10);
	ck_assert_int_ge(rc, 0);

	ck_assert_str_eq(buf, "async_test");
}

START_TEST(test_async_return)
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

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;
	TCase *tc_core;

	s = suite_create("lrpc");

	/* Core test case */
	tc_core = tcase_create("Core");
	tcase_set_timeout(tc_core, 0.1);
	tcase_add_test(tc_core, test_sync_return);
	tcase_add_test(tc_core, test_async_return);
	suite_add_tcase(s, tc_core);

	sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}