/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#ifndef __MATCHING_BROWSE_H_
#define __MATCHING_BROWSE_H_

#include <glib.h>
#include <xmmsclient/xmmsclient.h>

typedef struct browse_entry_St browse_entry_t;

void browse_entry_get (browse_entry_t *data, const gchar **url, gboolean *is_directory);
void browse_entry_free (browse_entry_t *entry);
GList *matching_browse (xmmsc_connection_t *conn, const gchar *path);

#endif
