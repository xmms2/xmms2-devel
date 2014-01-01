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

#include <stdio.h>
#include <stdarg.h>

#include "xmmscpriv/xmmsc_log.h"
#include "xmmscpriv/xmmsc_util.h"

static const char * log_level_to_str[XMMS_LOG_LEVEL_COUNT]
	= {"???", "FATAL", "FAIL", "ERROR", "INFO", "DEBUG"};

/**
 * Default xmmsc_log-handler
 * Writes the error message @msg to stderr, prepending @level and @domain if appropriate
 */
void
xmmsc_log_default_handler (const char *domain, xmmsc_log_level_t level, const char *msg)
{
	if (level < 0 || level >= XMMS_LOG_LEVEL_COUNT) {
		fprintf (stderr, "*** invalid log level! ***\n");
		level = XMMS_LOG_LEVEL_UNKNOWN;
	}

	if (domain && domain[0]) {
		fprintf (stderr, "%s in %s: %s\n", log_level_to_str[level], domain, msg);
	} else {
		fprintf (stderr, "%s: %s\n", log_level_to_str[level], msg);
	}

	xmms_dump_stack ();
}

static void
xmmsc_log_va (const char *domain, xmmsc_log_level_t level, const char *fmt, va_list ap)
{
	char *msg = x_vasprintf (fmt, ap);
	xmmsc_log_default_handler (domain, level, msg);
	free (msg);
}

/**
 * Log a message
 * @internal
 */
void
xmmsc_log (const char *domain, xmmsc_log_level_t level, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	xmmsc_log_va (domain, level, fmt, ap);
	va_end (ap);
}
