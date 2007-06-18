/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
 * Logging functions.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "xmmspriv/xmms_log.h"

static void xmms_log_handler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);


void
xmms_log_init (gint verbosity)
{
	g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL, xmms_log_handler,
	                   GINT_TO_POINTER (verbosity));

	xmms_log_info ("Initialized logging system :)");
}

void
xmms_log_shutdown ()
{
	xmms_log_info ("Logging says bye bye :)");
}


static void
xmms_log_handler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
	const char *level = "??";
	gint verbosity = GPOINTER_TO_INT (user_data);

	if (log_level & G_LOG_LEVEL_CRITICAL) {
		level = " FAIL";
	} else if (log_level & G_LOG_LEVEL_ERROR) {
		level = "FATAL";
	} else if (log_level & G_LOG_LEVEL_WARNING) {
		level = "ERROR";
	} else if (log_level & G_LOG_LEVEL_MESSAGE) {
		level = " INFO";
		if (verbosity < 1)
			return;
	} else if (log_level & G_LOG_LEVEL_DEBUG) {
		level = "DEBUG";
		if (verbosity < 2)
			return;
	}

	printf ("%s: %s\n", level, message);
	fflush (stdout);

	if (log_level & G_LOG_LEVEL_ERROR) {
		exit (EXIT_FAILURE);
	}
}
