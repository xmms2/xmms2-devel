/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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

#include "compat.h"

#include <windows.h>

gboolean
x_realpath (const gchar *item, gchar *rpath)
{
	return (GetFullPathNameA (item, XMMS_PATH_MAX, rpath, NULL) > XMMS_PATH_MAX);
}

gchar *
x_path2url (gchar *path)
{
	return g_strdelimit (path, "\\", '/');
}

gint
find_terminal_width ()
{
	HANDLE h_stdout;
	CONSOLE_SCREEN_BUFFER_INFO info;

	h_stdout = GetStdHandle (STD_OUTPUT_HANDLE);

	if (GetConsoleScreenBufferInfo (h_stdout, &info)) {
		return info.dwMaximumWindowSize.X;
	} else {
		return 80;
	}
}
