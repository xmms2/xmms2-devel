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
 *  Playlist readers / writers.
 *
 * This file contains functions for manipulate xmms_playlist_plugin_t objects.
 */

#include "xmmspriv/xmms_transport.h"
#include "xmmspriv/xmms_plsplugins.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmms/xmms_log.h"

#include <string.h>

/*
 * Static members
 */

static xmms_plugin_t * xmms_playlist_plugin_find (const gchar *mimetype);

/*
 * Public functions.
 */

GString *
xmms_playlist_plugin_save (gchar *mime,
			   guint32 *list)
{
	GString *ret;
	xmms_playlist_plugin_write_method_t write;
	xmms_plugin_t *plugin;

	g_return_val_if_fail (mime, NULL);
	g_return_val_if_fail (list, NULL);
	
	plugin = xmms_playlist_plugin_find (mime);
	if (!plugin) {
		return NULL;
	}

	write = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_WRITE_PLAYLIST);
	
	ret = write (list);
	xmms_object_unref (plugin);
	return ret;
}

gboolean
xmms_playlist_plugin_import (guint playlist_id, xmms_medialib_entry_t entry)
{
	gboolean ret;
	xmms_transport_t *transport;
	xmms_playlist_plugin_read_method_t read_method;
	xmms_plugin_t *plugin;
	const gchar *mime;

	transport = xmms_transport_new ();
	if (!transport)
		return FALSE;

	if (!xmms_transport_open (transport, entry)) {
		xmms_object_unref (XMMS_OBJECT (transport));
		return FALSE;
	}

	xmms_transport_start (transport);

	xmms_transport_buffering_start (transport);

	mime = xmms_transport_mimetype_get_wait (transport);
	if (!mime) {
		xmms_object_unref (XMMS_OBJECT (transport));
		return FALSE;
	}

	plugin = xmms_playlist_plugin_find (mime);
	if (!plugin) {
		xmms_object_unref (XMMS_OBJECT (transport));
		return FALSE;
	}

	read_method = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST);
	if (!read_method) {
		xmms_object_unref (transport);
		xmms_object_unref (plugin);
		return FALSE;
	}

	ret = read_method (transport, playlist_id);
	xmms_object_unref (transport);
	xmms_object_unref (plugin);

	return ret;
}

/*
 * Static members
 */

static xmms_plugin_t *
xmms_playlist_plugin_find (const gchar *mimetype)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;
	xmms_playlist_plugin_can_handle_method_t can_handle;
	
	g_return_val_if_fail (mimetype, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_PLAYLIST);
	
	for (node = list; node; node = g_list_next (node)) {
		plugin = node->data;
		XMMS_DBG ("Trying plugin: %s", xmms_plugin_name_get (plugin));
		can_handle = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE);
		if (!can_handle)
			continue;

		if (can_handle (mimetype)) {
			xmms_object_ref (plugin);
			break;
		}

	}

	if (!node)
		plugin = NULL;

	if (list)
		xmms_plugin_list_destroy (list);
	
	return plugin;
}
