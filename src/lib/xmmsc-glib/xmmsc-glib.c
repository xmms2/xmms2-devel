/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

#include "xmmsc/xmmsc-glib.h"
#include <glib.h>

static const GLogLevelFlags log_level_to_glib[XMMS_LOG_LEVEL_COUNT]
	= {G_LOG_LEVEL_CRITICAL, G_LOG_LEVEL_ERROR, G_LOG_LEVEL_CRITICAL,
	   G_LOG_LEVEL_WARNING, G_LOG_LEVEL_MESSAGE, G_LOG_LEVEL_DEBUG};

/**
 * Convert xmmsc_log-messages to g_log-messages
 *
 * Uses the same domain and the following log_level correspondence: unknown ->
 * critical, fatal -> error, fail -> critical, error -> warning, info ->
 * message, debug -> debug.
 *
 * @see xmmsc_log_handler_set
 */
void
xmmsc_log_glib_handler (const char *domain, xmmsc_log_level_t level, const char *msg, void *unused)
{
	if (level < 0 || level >= XMMS_LOG_LEVEL_COUNT) {
		g_critical ("invalid log level!");
		level = XMMS_LOG_LEVEL_UNKNOWN;
	}

	g_log (domain, log_level_to_glib[level], "%s", msg);
}
