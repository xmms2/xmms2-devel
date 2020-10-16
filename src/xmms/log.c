/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <xmmspriv/xmms_log.h>
#include <xmmspriv/xmms_localtime.h>
#include <xmmsc/xmmsc_log.h>
#include <xmmsc/xmmsc-glib.h>

static gchar *logts_format = NULL;
static void xmms_log_handler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);


void
xmms_log_set_format (const gchar *format)
{
	/* copying the string ensures the formatting directives will
	 * last until log_shutdown */
	g_free (logts_format);
	logts_format = g_strdup (format);
}

void
xmms_log_init (gint verbosity)
{
	xmmsc_log_handler_set (xmmsc_log_glib_handler, NULL);
	g_log_set_default_handler (xmms_log_handler, GINT_TO_POINTER (verbosity));
	xmms_log_info ("Initialized logging system :)");
}

void
xmms_log_shutdown ()
{
	xmms_log_info ("Logging says bye bye :)");
	g_free (logts_format);
	logts_format = NULL;
}


static void
xmms_log_handler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data)
{
	gchar logts_buf[256];
	time_t tv = 0;
	struct tm st;
	const char *level = "??";
	gint verbosity = GPOINTER_TO_INT (user_data);

	tv = time (NULL);

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

	if (!logts_format ||
	    !xmms_localtime (&tv, &st) ||
	    !strftime (logts_buf, sizeof(logts_buf), logts_format, &st)) {
		logts_buf[0] = '\0';
	}

	if (log_domain && log_domain[0]) {
		printf ("%s%s in %s: %s\n", logts_buf, level, log_domain, message);
	} else {
		printf ("%s%s: %s\n", logts_buf, level, message);
	}

	fflush (stdout);

	if (log_level & G_LOG_LEVEL_ERROR) {
		exit (EXIT_FAILURE);
	}
}
