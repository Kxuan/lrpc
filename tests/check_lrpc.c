/*
 */
#include <unistd.h>
#include <check.h>
#include <lrpc.h>
#include <interface.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

char name_provider[] = "provider";
char name_invoker[] = "invoker";

lrpc_return_status provider_echo(void *user_data,
                                 void *args, size_t args_len,
                                 void *ret_val, size_t *ret_size)
{
	memcpy(ret_val, args, args_len);
	*ret_size = args_len;

	return LRPC_RETURN_VAL;
}

void provider(int sigfd)
{
	int rc;
	struct lrpc_interface inf;
	struct lrpc_method method;

	lrpc_init(&inf, name_provider, sizeof(name_provider));
	lrpc_method_init(&method, "echo", provider_echo, NULL);

	rc = lrpc_register_method(&inf, &method);
	ck_assert_int_ge(rc, 0);

	rc = lrpc_start(&inf);
	ck_assert_int_ge(rc, 0);

	write(sigfd, "", 1);
	rc = lrpc_poll(&inf);
	ck_assert_int_ge(rc, 0);

}

void invoker(int sigfd)
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

START_TEST(test_call)
	int rc;
	pid_t pid;
	int fds[2];

	rc = pipe(fds);

	ck_assert_int_ge(rc, 0);

	pid = fork();
	ck_assert_int_ge(pid, 0);

	if (pid == 0) {
		provider(fds[1]);
		fprintf(stderr, "provider done\n");
		exit(0);
	} else {
		invoker(fds[0]);
		fprintf(stderr, "invoker done\n");
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
	tcase_add_test(tc_core, test_call);
	suite_add_tcase(s, tc_core);

	sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}