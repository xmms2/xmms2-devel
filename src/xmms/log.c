/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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
 * Based on gstp_log by Alexander Haväng.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "xmms/log.h"
#include "xmms/config.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <time.h>

#include <glib.h>
#include <sys/types.h>
#include <signal.h>

#include <regex.h>

#ifndef BUFSIZE
#define BUFSIZE 8192
#endif

/* Private function declarations */
static void xmms_log_string_with_time (gint loglevel, const char *format, 
									   const char *string);
static int xmms_log_open_logfile ();
static int xmms_log_close_logfile ();
char *xmms_log_filename = NULL;
static FILE *xmms_log_stream = NULL;
static int xmms_log_daemonized = 0;

xmms_config_value_t *regex_cv = NULL;
regex_t re;

static void
xmms_log_config_regex_changed (xmms_object_t *object, gconstpointer data,
							   gpointer userdata)
{
	int err;
	
	xmms_log_debug ("XMMS_LOG_CONFIG_REGEX_CHANGED");
	g_return_if_fail (data);

	regfree (&re);

	err = regcomp (&re, data, REG_EXTENDED);
	if (err != 0) {
		xmms_log_error ("Unable to compile regular expression");
	}
}

/** @defgroup Log Log
  * @ingroup XMMSServer
  * @{
  */

/** Initialize the log module, creates log file */
gint
xmms_log_init (const gchar *filename)
{
	if (strcmp (filename, "stdout") == 0) {
		xmms_log_filename = g_strdup (filename);
		xmms_log_stream = stdout;
	} else if (strcmp (filename, "stderr") == 0) {
		xmms_log_stream = stderr;
		xmms_log_filename = g_strdup (filename);
	} else if (strcmp (filename, "null") == 0) {
		xmms_log_stream = NULL;
		xmms_log_filename = g_strdup (filename);
	} else {

		xmms_log_filename = g_strdup_printf ("/tmp/%s.%d", filename, 
											 (int)time (NULL));
		  
		if (!xmms_log_open_logfile ()) {
			return 0;
		}
	}

	xmms_config_value_register ("log.regexp", ".*", NULL, NULL);
	regex_cv = xmms_config_lookup ("log.regexp");
	xmms_config_value_callback_set (regex_cv, xmms_log_config_regex_changed,
									NULL);
	
	xmms_log_config_regex_changed ((xmms_object_t *)regex_cv, 
					(gconstpointer *)xmms_config_value_string_get (regex_cv), 
					NULL);
	
	return 1;
}

/** Shutdown the log module, close the logfile */
void
xmms_log_shutdown (void)
{
	xmms_log_close_logfile ();

	if (xmms_log_filename)
		g_free (xmms_log_filename);

	regfree (&re);
}

/** Close everything, start up with clean slate when 
 * run as a daemon */
void
xmms_log_daemonize (void)
{
#ifndef _WIN32
	close (0); 
	close (1); 
	close (2);

	freopen ("/dev/null", "r", stdin);
	freopen ("/dev/null", "w", stdout);
	freopen ("/dev/null", "w", stderr);

	xmms_log_daemonized = 1;
#endif
}

/** Log only if verbose mode is set. Prepend output with DEBUG:  */
void
xmms_log_debug (const gchar *fmt, ...)
{
	char buff[BUFSIZE];
	va_list ap;

	va_start (ap, fmt);
#ifdef HAVE_VSNPRINTF
	vsnprintf (buff, BUFSIZE, fmt, ap);
#else
	vsprintf (buff, fmt, ap);
#endif
	va_end (ap);

	xmms_log_string_with_time (XMMS_LOG_DEBUG, "DEBUG: %s\n", buff);
}

/** Log to console and file */
void
xmms_log (const char *fmt, ...)
{
	char buff[BUFSIZE];
	va_list ap;

	va_start (ap, fmt);
	g_vsnprintf (buff, BUFSIZE, fmt, ap);
	va_end (ap);

	xmms_log_string_with_time (XMMS_LOG_INFORMATION, "%s\n", buff);
}

/** Fatal log message. Terminate server */
void
xmms_log_fatal (const gchar *fmt, ...)
{
	char buff[BUFSIZE];
	va_list ap;

	va_start (ap, fmt);
	g_vsnprintf (buff, BUFSIZE, fmt, ap);
	va_end (ap);

	xmms_log_string_with_time (XMMS_LOG_FATAL, "%s\n", buff);
	xmms_log_string_with_time (XMMS_LOG_FATAL, "%s\n", "Shutting down...");
	
	exit (0);
}

static void
xmms_log_string_with_time (gint loglevel, const gchar *format, const gchar *string)
{
	struct tm *tp;
#ifdef HAVE_LOCALTIME_R
	struct tm tms;
#endif
	time_t now;
	gchar timestring[20];
	
	int err = REG_NOMATCH;
	regmatch_t pm;
	
	err = regexec (&re, string, 0, &pm, 0);
	if (err == REG_NOMATCH)
		return;
	
	/* send the information to core */
	/*msg = g_strdup_printf (format, string);
	xmms_core_information (loglevel, msg);
	g_free (msg);*/
	
	if (xmms_log_stream) {
		now = time (NULL);
			
#ifdef HAVE_LOCALTIME_R
		tp = localtime_r (&now, &tms);
#else
		/* this is not threadsafe, I know */
		tp = localtime (&now);
#endif
		if (!tp)
			return;
			
		strftime (timestring, 20, "%Y-%m-%d %H:%M", tp);
		if (timestring[strlen (timestring) - 1] == '\n')
			timestring[strlen (timestring) - 1] = '\0';
			
		fprintf (xmms_log_stream, "%s ", timestring);
		fprintf (xmms_log_stream, format, string);
			
#ifndef HAVE_SETLINEBUF
		fflush (xmms_log_stream);
#endif
	}
		
	/* Don't log to console when daemonized */
	if (!xmms_log_daemonized && (xmms_log_stream != stdout)) {
		now = time (NULL);
			
#ifdef HAVE_LOCALTIME_R
		tp = localtime_r (&now, &tms);
#else
		tp = localtime (&now);
#endif
			
		if (!tp)
			return;
			
		strftime (timestring, 20, "%Y-%m-%d %H:%M", tp);
		fprintf (stdout, "%s ", timestring);
		fprintf (stdout, format, string);
	}
}

static int
xmms_log_open_logfile ()
{
	FILE *logfp;

	logfp = fopen (xmms_log_filename, "w+");

	if (!logfp) {
		fprintf (stderr, "Error while opening %s, error: %s\n", 
				 xmms_log_filename, strerror (errno));
		return 0;
	}

	xmms_log_stream = logfp;
#ifdef HAVE_SETLINEBUF
	setlinebuf (xmms_log_stream);
#endif

	return 1;
}

static int
xmms_log_close_logfile ()
{
	if (xmms_log_stream) {
		fclose (xmms_log_stream);
	}

	return 1;
}

/** @} */
