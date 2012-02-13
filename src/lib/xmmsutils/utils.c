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

/** @file
 * Miscellaneous internal utility functions.
 */

#include <stdlib.h>
#include <string.h>

#include "xmms_configuration.h"
#include "xmmsc/xmmsc_util.h"

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
