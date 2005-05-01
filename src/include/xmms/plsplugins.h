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




#ifndef __XMMS_PLAYLIST_PLUGIN_H__
#define __XMMS_PLAYLIST_PLUGIN_H__


#include "xmms/transport.h"
#include "xmms/playlist.h"
#include "xmms/medialib.h"

/*
 * Plugin methods.
 */

typedef gboolean (*xmms_playlist_plugin_read_method_t) (xmms_transport_t *transport, guint playlist_id);
typedef gboolean (*xmms_playlist_plugin_can_handle_method_t) (const gchar *mimetype);
typedef GString *(*xmms_playlist_plugin_write_method_t) (guint32 *list);

/*
 * Public functions.
 */

gboolean xmms_playlist_plugin_import (guint playlist_id, xmms_medialib_entry_t entry);
GString *xmms_playlist_plugin_save (gchar *mime, guint32 *list);

#endif
