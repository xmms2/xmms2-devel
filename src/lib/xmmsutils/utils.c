/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

/** @file
 * Miscellaneous internal utility functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xmms_configuration.h>
#include <xmmsc/xmmsc_util.h>
#include <xmmscpriv/xmmsc_util.h>

/**
 * Get the default connection path.
 *
 * @param buf A char buffer
 * @param len The length of buf (XMMS_PATH_MAX is a good choice)
 * @return A pointer to buf, or NULL if an error occured.
 */
const char *
xmms_default_ipcpath_get (char *buf, int len)
{
	const char *xmmspath;

	xmmspath = getenv ("XMMS_PATH");
	if (xmmspath && strlen (xmmspath) < len) {
		strcpy (buf, xmmspath);
	} else {
		return xmms_fallback_ipcpath_get (buf, len);
	}

	return buf;
}

/**
 * vsprintf, but with x_new allocated result
 */
char *
x_vasprintf (const char *fmt, va_list args)
{
	char c;
	char *res;
	int bound;
	va_list ap;

	x_return_null_if_fail (fmt);

	va_copy (ap, args);
	bound = vsnprintf (&c, 1, fmt, ap);
	va_end (ap);

	x_return_null_if_fail (bound >= 0)

	res = x_new (char, bound + 1);
	vsnprintf (res, bound+1, fmt, args);

	return res;
}

/**
 * sprintf, but with x_new allocated result
 */
char *
x_asprintf (const char *fmt, ...)
{
	va_list ap;
	char *res;

	va_start (ap, fmt);
	res = x_vasprintf (fmt, ap);
	va_end (ap);

	return res;
}
