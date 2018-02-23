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
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <lrpc.h>
#include "test_common.h"

#define THREAD_COUNT 100
#define TEST_COUNT 2000
#define INVOKER_COUNT 21

pthread_mutex_t lock_counter = PTHREAD_MUTEX_INITIALIZER;
static size_t counter = 0;

static int sync_rpc_echo(void *user_data, const struct lrpc_callback_ctx *ctx)
{
	void *args = NULL;
	size_t args_len = 0;

	lrpc_get_args(ctx, &args, &args_len);

	ck_assert_str_eq(args, TEST_CONTENT);
	ck_assert_int_eq(args_len, sizeof(TEST_CONTENT));

	lrpc_return(ctx, args, args_len);

	return 0;
}

static int sync_rpc_exit(void *user_data, const struct lrpc_callback_ctx *ctx)
{
	*(int *) user_data = 0;
	lrpc_return(ctx, NULL, 0);
	return 0;
}

static void provider(int sigfd)
{
	int rc;
	ssize_t size;
	struct lrpc_interface inf;
	struct lrpc_method method_echo, method_exit;
	int running = 1;

	lrpc_init(&inf, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	lrpc_method_init(&method_echo, TEST_METHOD, sync_rpc_echo, NULL);
	lrpc_method_init(&method_exit, "exit", sync_rpc_exit, &running);

	rc = lrpc_method(&inf, &method_echo);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_method(&inf, &method_exit);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_start(&inf);
	ck_assert_int_ge(rc, 0);

	size = write(sigfd, "", 1);
	ck_assert_int_ge(size, 0);

	while (running) {
		rc = lrpc_poll(&inf);
		if (rc != 0) {
			perror("lrpc_poll");
		}
		ck_assert_int_ge(rc, 0);
	}

	lrpc_stop(&inf);
}

static void *thread_poll_routine(void *user_data)
{
	int rc;
	struct lrpc_interface *all_inf = user_data, *inf;
	int epoll_fd;
	int nevents;
	int i;
	struct epoll_event evt[INVOKER_COUNT];

	epoll_fd = epoll_create(1);
	ck_assert_int_ge(epoll_fd, 0);
	for (i = 0; i < INVOKER_COUNT; ++i) {
		evt[i].events = EPOLLIN;
		evt[i].data.ptr = all_inf + i;

		rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, all_inf[i].fd, evt + i);
		ck_assert_int_eq(rc, 0);
	}

	while (1) {
		nevents = epoll_wait(epoll_fd, evt, sizeof(evt) / sizeof(*evt), -1);
		for (i = 0; i < nevents; i++) {
			inf = evt[i].data.ptr;
			rc = lrpc_poll(inf);
			ck_assert_int_eq(rc, 0);
		}
	}

}

static void *sync_invoker_routine(void *user_data)
{
	int rc;
	char buf[sizeof(TEST_CONTENT)];
	struct lrpc_endpoint *peer = user_data;

	for (int i = 0; i < TEST_COUNT; ++i) {
		memset(buf, 0, sizeof(buf));
		rc = (int) lrpc_call(peer, TEST_METHOD, TEST_CONTENT, sizeof(TEST_CONTENT), buf, sizeof(buf));
		ck_assert_int_ge(rc, 0);
		ck_assert_str_eq(buf, TEST_CONTENT);

		pthread_mutex_lock(&lock_counter);
		counter++;
		pthread_mutex_unlock(&lock_counter);
	}

}

static void async_echo_callback(struct lrpc_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
{
	ck_assert_int_eq(err_code, 0);
	ck_assert_str_eq(ret_ptr, TEST_CONTENT);

	pthread_mutex_lock(&lock_counter);
	counter++;
	pthread_mutex_unlock(&lock_counter);

	sem_post(ctx->user_data);
}

static void *async_invoker_routine(void *user_data)
{
	int rc;
	char buf[sizeof(TEST_CONTENT)];
	struct lrpc_call_ctx ctx;
	struct lrpc_endpoint *peer = user_data;
	sem_t signal;

	sem_init(&signal, 0, 0);
	ctx.user_data = &signal;

	for (int i = 0; i < TEST_COUNT; ++i) {
		memset(buf, 0, sizeof(buf));
		rc = lrpc_call_async(peer, &ctx, TEST_METHOD, TEST_CONTENT, sizeof(TEST_CONTENT), async_echo_callback);
		ck_assert_int_eq(rc, 0);

		rc = sem_wait(&signal);
		ck_assert_int_eq(rc, 0);
	}
	sem_destroy(&signal);
}

static void async_exit_callback(struct lrpc_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
{
	ck_assert_ptr_ne(ctx, NULL);
	ck_assert_int_eq(err_code, 0);
	ck_assert_int_eq(ret_size, 0);
}

static pthread_t start_sync_invoker(struct lrpc_endpoint *peer)
{
	pthread_t thread;
	int rc;

	rc = pthread_create(&thread, NULL, sync_invoker_routine, peer);
	ck_assert_int_eq(rc, 0);

	return thread;
}

static pthread_t start_async_invoker(struct lrpc_endpoint *peer)
{
	pthread_t thread;
	int rc;

	rc = pthread_create(&thread, NULL, async_invoker_routine, peer);
	ck_assert_int_eq(rc, 0);

	return thread;
}

void init_invoker(struct lrpc_interface *inf, struct lrpc_endpoint *peer)
{
	int rc;
	char invoker_name[sizeof(NAME_INVOKER) + 2 * sizeof(void *)];
	int name_len;

	name_len = snprintf(invoker_name, sizeof(invoker_name), "%s%p", NAME_INVOKER, inf);
	lrpc_init(inf, invoker_name, (size_t) name_len);

	rc = lrpc_start(inf);
	ck_assert_int_eq(rc, 0);

	rc = lrpc_connect(inf, peer, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	ck_assert_int_ge(rc, 0);

}

static void run_invoker()
{
	int rc;
	int i;
	pthread_t thread_poll, thread_list[THREAD_COUNT];
	void *thread_rc;
	struct lrpc_interface invoker[INVOKER_COUNT];
	struct lrpc_endpoint peer[INVOKER_COUNT];

	for (i = 0; i < INVOKER_COUNT; i++) {
		init_invoker(invoker + i, peer + i);
	}
	rc = pthread_create(&thread_poll, NULL, thread_poll_routine, invoker);
	ck_assert_int_eq(rc, 0);

	for (i = 0; i < THREAD_COUNT; ++i) {
		if (i % 2) {
			thread_list[i] = start_sync_invoker(peer + (i % INVOKER_COUNT));
		} else {
			thread_list[i] = start_async_invoker(peer + (i % INVOKER_COUNT));
		}
		ck_assert_int_ne(thread_list[i], 0);
	}

	for (i = 0; i < THREAD_COUNT; ++i) {
		rc = pthread_join(thread_list[i], &thread_rc);
		ck_assert_int_eq(rc, 0);
		ck_assert_ptr_eq(thread_rc, NULL);
	}

	rc = (int) lrpc_call(peer + 0, "exit", NULL, 0, NULL, 0);
	ck_assert_int_ge(rc, 0);

	pthread_cancel(thread_poll);
	rc = pthread_join(thread_poll, NULL);
	ck_assert_int_eq(rc, 0);
}

START_TEST(test_thread)
	int rc;
	int fds[2];
	pid_t pid;
	ssize_t size;
	char buf;

	errno = 0;

	rc = pipe(fds);

	ck_assert_int_ge(rc, 0);

	pid = fork();
	ck_assert_int_ge(pid, 0);

	if (pid == 0) {
		close(fds[0]);
		provider(fds[1]);
		exit(0);
	}

	close(fds[1]);
	size = read(fds[0], &buf, sizeof(buf));
	ck_assert_int_eq(size, 1);

	run_invoker();

	rc = waitpid(pid, NULL, 0);
	ck_assert_int_eq(rc, pid);

	ck_assert_int_eq(counter, THREAD_COUNT * TEST_COUNT);
END_TEST

int main()
{
	TCase *tc;

	int number_failed;
	Suite *s;
	SRunner *sr;

	s = suite_create(TEST_NAME);

	/* Multi thread test case */
	tc = tcase_create("Multi-thread");
	tcase_set_timeout(tc, 5);

	tcase_add_test(tc, test_thread);


	suite_add_tcase(s, tc);

	sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}