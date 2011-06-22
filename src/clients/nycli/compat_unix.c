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

#include "compat.h"

#include <limits.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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
find_terminal_width (void)
{
	gint columns;
	struct winsize ws;
	char *colstr, *endptr;

	if (!isatty (STDOUT_FILENO)) {
#ifdef LINE_MAX
		columns = LINE_MAX;
#else
		columns = 2048 /* Minimum value for LINE_MAX according to POSIX */
#endif
#ifdef TIOCGWINSZ
	} else if (!ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws)) {
		columns = ws.ws_col;
#endif
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
