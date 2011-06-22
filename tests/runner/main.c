/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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
#include "xcu.h"
#include <unistd.h>
#include <signal.h>

#include <xcu_valgrind.h>

static void __registry_init (void) __attribute__ ((constructor (200)));
static void __registry_init (void) {
	if (CU_initialize_registry() != CUE_SUCCESS)
		_exit (CU_get_error());

}

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

	memset (&sa, 0 , sizeof (sa));
	sa.sa_handler = segvhandler;
	sa.sa_flags = SA_RESETHAND;
	sigaction (SIGSEGV, &sa, NULL);

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();

	return CU_get_error();
}
