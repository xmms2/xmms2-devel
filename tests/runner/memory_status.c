/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include "memory_status.h"

#if defined(HAVE_VALGRIND) && HAVE_VALGRIND == 1

#include <valgrind.h>
#include <memcheck.h>

static int _memory_baseline_leaks;
static int _memory_baseline_errors;

void
memory_status_calibrate (const char *marker)
{
	int l, d, r, s;

	VALGRIND_PRINTF ("Calibrating: %s\n", marker);

	VALGRIND_DO_LEAK_CHECK;
	VALGRIND_COUNT_LEAKS(l, d, r, s);
	_memory_baseline_leaks = l;

	_memory_baseline_errors = VALGRIND_COUNT_ERRORS;
}

int
memory_status_verify (const char *marker)
{
	int l, d, r, s, status;

	VALGRIND_PRINTF ("Verifying: %s\n", marker);

	status = MEMORY_OK;

	if (VALGRIND_COUNT_ERRORS > _memory_baseline_errors)
		status |= MEMORY_ERROR;

	VALGRIND_DO_LEAK_CHECK;
	VALGRIND_COUNT_LEAKS(l, d, r, s);

	if (l > _memory_baseline_leaks)
		status |= MEMORY_LEAK;

	return status;
}

#else

void memory_status_calibrate (const char *marker)
{
}

int memory_status_verify (const char *marker)
{
	return MEMORY_OK;
}

#endif
