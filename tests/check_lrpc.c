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

TCase *create_tcase_core();
TCase *create_tcase_thread();

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = suite_create("lrpc");

	suite_add_tcase(s, create_tcase_core());
	suite_add_tcase(s, create_tcase_thread());

	sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}