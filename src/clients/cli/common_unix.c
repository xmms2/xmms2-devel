/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include <sys/ioctl.h>
#include <termios.h>

#include "common.h"

gboolean
x_realpath (const gchar *item, gchar *rpath)
{
	return !!realpath (item, rpath);
}

gchar *
x_path2url (gchar *path)
{
	return path;
}

gint
find_terminal_width ()
{
	gint columns = 0;
	struct winsize ws;
	char *colstr, *endptr;

	if (!ioctl (STDIN_FILENO, TIOCGWINSZ, &ws)) {
		columns = ws.ws_col;
	} else {
		colstr = getenv ("COLUMNS");
		if (colstr != NULL) {
			columns = strtol (colstr, &endptr, 10);
		}
	}

	/* Default to 80 columns */
	if (columns <= 0) {
		columns = 80;
	}

	return columns;
}
