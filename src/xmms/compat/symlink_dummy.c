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


/** @file
 * Dummy used when symlinking not available. (could possibly copy the file)
 */


#include "xmmspriv/xmms_symlink.h"

gboolean
xmms_symlink_file (gchar *source, gchar *dest)
{
	gchar *buf;
	gsize len;
	gboolean ret = FALSE;

	if (g_file_get_contents (source, &buf, &len, NULL)) {
		ret = g_file_set_contents (dest, buf, len, NULL);
		g_free (buf);
	}

	return ret;
}
