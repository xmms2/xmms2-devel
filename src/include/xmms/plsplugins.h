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

/* Type definitions */

typedef struct xmms_playlist_plugin_St {
	xmms_plugin_t *plugin;
} xmms_playlist_plugin_t;

/*
 * Plugin methods.
 */

typedef gboolean (*xmms_playlist_plugin_read_method_t) (xmms_playlist_plugin_t *plsplugin, xmms_transport_t *transport, xmms_playlist_t *playlist);
typedef gboolean (*xmms_playlist_plugin_can_handle_method_t) (const gchar *mimetype);
typedef gboolean (*xmms_playlist_plugin_write_method_t) (xmms_playlist_plugin_t *plsplugin, xmms_playlist_t *playlist, gchar *filename);

/*
 * Public functions.
 */

xmms_playlist_plugin_t *xmms_playlist_plugin_new (const gchar *mimetype);
void xmms_playlist_plugin_free (xmms_playlist_plugin_t *plsplugin);
void xmms_playlist_plugin_read (xmms_playlist_plugin_t *plsplugin, xmms_playlist_t *playlist, xmms_transport_t *transport);
gboolean xmms_playlist_plugin_save (xmms_playlist_plugin_t *plsplugin, xmms_playlist_t *playlist, gchar *filename);


#endif
