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

#include <xcu.h>
#include "xcu_valgrind.h"

#if defined(HAVE_VALGRIND) && HAVE_VALGRIND == 1

#include <valgrind.h>
#include <memcheck.h>


static int pre_test_leaks;

void
xcu_valgrind_pre_case ()
{
	int l, d, r, s;

	VALGRIND_DO_LEAK_CHECK;
	VALGRIND_COUNT_LEAKS(l, d, r, s);
	pre_test_leaks = l;
}

void
xcu_valgrind_post_case ()
{
	int l, d, r, s;

	VALGRIND_DO_LEAK_CHECK;
	VALGRIND_COUNT_LEAKS(l, d, r, s);
	if (l > pre_test_leaks) {
		CU_FAIL ("Memory leak detected");
	}

}

#else

void xcu_valgrind_pre_case () {}
void xcu_valgrind_post_case () {}

#endif
