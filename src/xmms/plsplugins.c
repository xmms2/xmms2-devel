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
#include "xmmspriv/xmms_magic.h"
#include "xmms/xmms_log.h"

#include <string.h>

/*
 * Static members
 */

static xmms_plugin_t *xmms_playlist_plugin_find_by_contents (xmms_transport_t *transport);
static xmms_plugin_t *xmms_playlist_plugin_find_by_mime (const gchar *mime);

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
	
	plugin = xmms_playlist_plugin_find_by_mime (mime);
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

	transport = xmms_transport_new ();
	if (!transport)
		return FALSE;

	if (!xmms_transport_open (transport, entry)) {
		xmms_object_unref (XMMS_OBJECT (transport));
		return FALSE;
	}

	xmms_transport_start (transport);

	xmms_transport_buffering_start (transport);

	plugin = xmms_playlist_plugin_find_by_contents (transport);
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
	xmms_transport_stop (transport);
	xmms_object_unref (transport);
	xmms_object_unref (plugin);

	return ret;
}

gboolean
xmms_playlist_plugin_verify (xmms_plugin_t *plugin)
{
	gboolean r, w;

	g_return_val_if_fail (plugin, FALSE);

	/* methods needed for read support */
	r = xmms_plugin_has_methods (plugin,
	                             XMMS_PLUGIN_METHOD_READ_PLAYLIST,
	                             NULL);

	/* methods needed for write support */
	w = xmms_plugin_has_methods (plugin,
	                             XMMS_PLUGIN_METHOD_WRITE_PLAYLIST,
	                             NULL);

	return r || w;
}

/*
 * Static members
 */
static gint
cb_sort_plugin_list (xmms_plugin_t *a, xmms_plugin_t *b)
{
	guint n1, n2;

	n1 = xmms_magic_complexity (xmms_plugin_magic_get (a));
	n2 = xmms_magic_complexity (xmms_plugin_magic_get (b));

	if (n1 > n2) {
		return -1;
	} else if (n1 < n2) {
		return 1;
	} else {
		return 0;
	}
}

static xmms_plugin_t *
xmms_playlist_plugin_find_by_contents (xmms_transport_t *transport)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;
	xmms_magic_checker_t c;

	g_return_val_if_fail (transport, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_PLAYLIST);
	if (!list) {
		return NULL;
	}

	/* sort by complexity of the magic trees, so plugins that accept
	 * any data are checked last
	 */
	list = g_list_sort (list, (GCompareFunc) cb_sort_plugin_list);

	c.transport = transport;
	c.read = 0;
	c.alloc = 128; /* start with a 128 bytes buffer */
	c.buf = g_malloc (c.alloc);

	for (node = list; node; node = g_list_next (node)) {
		const GList *magic;

		plugin = node->data;
		magic = xmms_plugin_magic_get (plugin);

		XMMS_DBG ("performing magic check for %s",
		          xmms_plugin_shortname_get (plugin));
		if (!magic || xmms_magic_match (&c, magic)) {
			xmms_object_ref (plugin);
			break;
		}
	}

	g_free (c.buf);

	xmms_plugin_list_destroy (list);

	return node ? plugin : NULL;
}

static xmms_plugin_t *
xmms_playlist_plugin_find_by_mime (const gchar *mimetype)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;

	g_return_val_if_fail (mimetype, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_PLAYLIST);
	if (!list) {
		return NULL;
	}

	for (node = list; node; node = g_list_next (node)) {
		const GList *magic, *node2;

		plugin = node->data;
		magic = xmms_plugin_magic_get (plugin);

		XMMS_DBG ("Trying plugin: %s", xmms_plugin_name_get (plugin));

		for (node2 = magic; node2; node2 = g_list_next (node2)) {
			GNode *tree = node2->data;
			gpointer *data = tree->data;

			if (!g_strcasecmp ((gchar *) data[1], mimetype)) {
				xmms_object_ref (plugin);
				goto out;
			}
		}
	}

out:
	xmms_plugin_list_destroy (list);

	return node ? plugin : NULL;
}
