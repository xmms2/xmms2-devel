/** @file
 *  Playlist readers / writers.
 *
 * This file contains functions for manipulate xmms_playlist_plugin_t objects.
 */

#include "plugin.h"
#include "plugin_int.h"
#include "util.h"
#include "playlist.h"
#include "plsplugins.h"

#include <string.h>

/*
 * Static members
 */

static xmms_plugin_t * xmms_playlist_plugin_find (const gchar *mimetype);

/*
 * Public functions.
 */

gboolean
xmms_playlist_plugin_save (xmms_playlist_plugin_t *plsplugin,
		xmms_playlist_t *playlist, gchar *filename)
{
	xmms_playlist_plugin_write_method_t write;

	g_return_val_if_fail (plsplugin, FALSE);
	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (filename, FALSE);

	write = xmms_plugin_method_get (plsplugin->plugin, XMMS_PLUGIN_METHOD_WRITE_PLAYLIST);
	
	return write (playlist, filename);
}

xmms_playlist_plugin_t *
xmms_playlist_plugin_new (const gchar *mimetype)
{
	xmms_plugin_t *plugin;
	xmms_playlist_plugin_t *plsplugin;
	
	g_return_val_if_fail (mimetype, NULL);

	plugin = xmms_playlist_plugin_find (mimetype);
	if (!plugin)
		return NULL;

	XMMS_DBG ("Playlist plugin %s wants this.", xmms_plugin_name_get (plugin));
	
	plsplugin = g_new0 (xmms_playlist_plugin_t, 1);
	plsplugin->plugin = plugin;

	return plsplugin;
}

void
xmms_playlist_plugin_free (xmms_playlist_plugin_t *plsplugin)
{
	xmms_plugin_unref (plsplugin->plugin);
	g_free (plsplugin);
}

void
xmms_playlist_plugin_read (xmms_playlist_plugin_t *plsplugin, xmms_playlist_t *playlist, xmms_transport_t *transport)
{
	xmms_playlist_plugin_read_method_t read_method;
	g_return_if_fail (plsplugin);
	g_return_if_fail (transport);

	read_method = xmms_plugin_method_get (plsplugin->plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST);
	g_return_if_fail (read_method);

	read_method (transport, playlist);
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
			xmms_plugin_ref (plugin);
			break;
		}

	}

	if (!node)
		plugin = NULL;

	if (list)
		xmms_plugin_list_destroy (list);
	
	return plugin;
}
