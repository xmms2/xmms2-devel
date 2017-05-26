/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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
 * Takes care of creating a symlink to a file.
 */


#include <xmmspriv/xmms_symlink.h>

#include <unistd.h>


gboolean
xmms_symlink_file (gchar *source, gchar *dest)
{
	gint r;

	g_return_val_if_fail (source, FALSE);
	g_return_val_if_fail (dest, FALSE);

	r = symlink (source, dest);

	return r != -1;
}
