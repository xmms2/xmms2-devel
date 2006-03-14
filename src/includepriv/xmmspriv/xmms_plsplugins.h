/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 Peter Alm, Tobias Rundström, Anders Gustafsson
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




#ifndef __XMMS_PRIV_PLAYLIST_PLUGIN_H__
#define __XMMS_PRIV_PLAYLIST_PLUGIN_H__

#include "xmms/xmms_plsplugins.h"
#include "xmms/xmms_medialib.h"
#include "xmmspriv/xmms_plugin.h"

gboolean xmms_playlist_plugin_import (guint playlist_id, xmms_medialib_entry_t entry);
GString *xmms_playlist_plugin_save (gchar *mime, guint32 *list);

gboolean xmms_playlist_plugin_verify (xmms_plugin_t *plugin);

#endif
