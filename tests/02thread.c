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
#include <semaphore.h>
#include "../src/msg.h"
#include "../src/socket.h"
#include "test_common.h"

#define THREAD_COUNT 100
#define TEST_COUNT 200
pthread_mutex_t lock_counter = PTHREAD_MUTEX_INITIALIZER;
static size_t counter = 0;

static int sync_rpc_echo(void *user_data, const struct lrpc_callback_ctx *ctx, void *args, size_t args_len)
{
	ck_assert_str_eq(args, TEST_CONTENT);
	ck_assert_int_eq(args_len, sizeof(TEST_CONTENT));

	lrpc_return(ctx, args, args_len);

	return 0;
}

static int sync_rpc_exit(void *user_data, const struct lrpc_callback_ctx *ctx, void *args, size_t args_len)
{
	*(int *) user_data = 0;
	lrpc_return(ctx, NULL, 0);
	return 0;
}

static void provider(int sigfd)
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_method method_echo, method_exit;
	struct lrpc_packet pkt_buffer;
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

	write(sigfd, "", 1);

	while (running) {
		rc = lrpc_poll(&inf, &pkt_buffer);
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
	struct lrpc_interface *inf = user_data;
	struct lrpc_packet rcv_buf;
	while (1) {
		rc = lrpc_poll(inf, &rcv_buf);
		ck_assert_int_eq(rc, 0);
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

static void async_echo_callback(struct lrpc_async_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
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
	struct lrpc_async_call_ctx ctx;
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

static void async_exit_callback(struct lrpc_async_call_ctx *ctx, int err_code, void *ret_ptr, size_t ret_size)
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

START_TEST(test_thread)
	int rc;
	int fds[2];
	int i;
	pid_t pid;
	ssize_t size;
	char buf;
	pthread_t thread_poll, thread_list[THREAD_COUNT];
	void *thread_rc;
	struct lrpc_interface inf;
	struct lrpc_endpoint peer;

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

	lrpc_init(&inf, NAME_INVOKER, sizeof(NAME_INVOKER));

	rc = lrpc_start(&inf);
	ck_assert_int_eq(rc, 0);

	rc = lrpc_connect(&inf, &peer, NAME_PROVIDER, sizeof(NAME_PROVIDER));
	ck_assert_int_ge(rc, 0);

	rc = pthread_create(&thread_poll, NULL, thread_poll_routine, &inf);
	ck_assert_int_eq(rc, 0);

	for (i = 0; i < THREAD_COUNT; ++i) {
		if (i % 2) {
			thread_list[i] = start_sync_invoker(&peer);
		} else {
			thread_list[i] = start_async_invoker(&peer);
		}
		ck_assert_ptr_ne(thread_list[i], NULL);
	}

	for (i = 0; i < THREAD_COUNT; ++i) {
		rc = pthread_join(thread_list[i], &thread_rc);
		ck_assert_int_eq(rc, 0);
		ck_assert_ptr_eq(thread_rc, NULL);
	}

	rc = (int) lrpc_call(&peer, "exit", NULL, 0, NULL, 0);
	ck_assert_int_ge(rc, 0);

	pthread_cancel(thread_poll);
	rc = pthread_join(thread_poll, NULL);
	ck_assert_int_eq(rc, 0);

	rc = waitpid(pid, NULL, 0);
	ck_assert_int_eq(rc, pid);

	ck_assert_int_eq(counter, THREAD_COUNT * TEST_COUNT);
END_TEST

TCase *create_tcase_thread()
{
	TCase *tc;

	/* Core test case */
	tc = tcase_create("Multi-thread");
	//tcase_set_timeout(tc, 0.1);

	tcase_add_test(tc, test_thread);
	return tc;
}