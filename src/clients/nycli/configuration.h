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

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <xmmsclient/xmmsclient.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "main.h"

struct configuration_St {
	gchar *path;
	GKeyFile *file;
	GHashTable *values;
};

/* FIXME(g): config or configuration? */
configuration_t *configuration_init (const gchar *path);
void configuration_free (configuration_t *config);

gboolean configuration_get_boolean (configuration_t *config, gchar *key);
gchar* configuration_get_string (configuration_t *config, gchar *key);

#endif /* __CONFIGURATION_H__ */
