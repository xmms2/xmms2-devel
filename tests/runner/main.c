/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include "xcu.h"
#include <unistd.h>
#include <signal.h>

#include <xcu_valgrind.h>

@@DECLARE_TEST_CASES@@

static void
segvhandler(int s)
{
	printf ("Testsuite received SIGSEGV, bad, really bad\n");

	raise (SIGSEGV);
}

int
xcu_pre_case (const char *name)
{
	xcu_valgrind_pre_case ();
	return 1;
}

void
xcu_post_case (const char *name)
{
	xcu_valgrind_post_case ();
}

int
main (int argc, char **argv)
{
	struct sigaction sa;
	unsigned int failures;

	if (CU_initialize_registry() != CUE_SUCCESS)
		return CU_get_error();

	memset (&sa, 0 , sizeof (sa));
	sa.sa_handler = segvhandler;
	sa.sa_flags = SA_RESETHAND;
	sigaction (SIGSEGV, &sa, NULL);

	@@REGISTER_TEST_SUITES@@

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();

	failures = CU_get_number_of_failures ();

	CU_cleanup_registry();

	if (failures > 0) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
