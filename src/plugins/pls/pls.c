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




#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/playlist.h"
#include "xmms/playlist_entry.h"
#include "xmms/plsplugins.h"
#include "xmms/util.h"
#include "xmms/xmms.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Type definitions
 */

/*
 * Function prototypes
 */

static gboolean xmms_pls_can_handle (const gchar *mimetype);
static gboolean xmms_pls_read_playlist (xmms_playlist_plugin_t *plsplugin,
                                        xmms_transport_t *transport,
                                        xmms_playlist_t *playlist);
static gboolean xmms_pls_write_playlist (xmms_playlist_plugin_t *plsplugin,
                                         xmms_playlist_t *playlist,
                                         gchar *filename);

static gchar * get_value (const gchar *pair, const gchar *key);
static gchar * build_encoded_url (const gchar *plspath, const gchar *file);
static gchar * path_get_body (const gchar *path);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_PLAYLIST, "pls",
	                          "PLS Playlist " XMMS_VERSION,
	                          "PLS Playlist reader / writer");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_pls_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST, xmms_pls_read_playlist);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE_PLAYLIST, xmms_pls_write_playlist);

	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_pls_can_handle (const gchar *mime)
{
	g_return_val_if_fail (mime, FALSE);

	XMMS_DBG ("xmms_pls_can_handle (%s)", mime);

	if ((g_strncasecmp (mime, "audio/x-scpls", 13) == 0))
		return TRUE;

	if ((g_strncasecmp (mime, "audio/scpls", 11) == 0))
		return TRUE;

	return FALSE;
}

static gboolean
xmms_pls_read_playlist (xmms_playlist_plugin_t *plsplugin,
                        xmms_transport_t *transport,
                        xmms_playlist_t *playlist)
{
	gchar *buffer;
	gchar *plspath;
	gchar **lines;
	guint current = 1;	/* Line that contains first "File" val */
	xmms_error_t error;
	gint read_bytes, size;

	g_return_val_if_fail (plsplugin, FALSE);
	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (playlist,  FALSE);

	size = xmms_transport_size (transport);

	if (size == 0) {
		return TRUE;
	}

	if (size == -1) {
		size = 4096;
	}

	buffer = g_malloc0 (size);
	g_return_val_if_fail (buffer, FALSE);

	read_bytes = 0;
	while (read_bytes < size) {
		gint ret = xmms_transport_read (transport, buffer + read_bytes,
		                                size - read_bytes, &error);

		XMMS_DBG ("Got %d bytes", ret);

		if (ret <= 0) {
			if (read_bytes > 0) {
				break;
			}

			g_free (buffer);
			return FALSE;
		}

		read_bytes += ret;
		g_assert (read_bytes >= 0);
	}

	lines = g_strsplit (buffer, "\n", 0);
	g_free (buffer);
	g_return_val_if_fail (lines, FALSE);

	if (lines[0] == NULL) {
		return FALSE;
	}

	if (g_ascii_strncasecmp (lines[0], "[playlist]", 10) != 0) {
		g_strfreev (lines);
		XMMS_DBG ("Not a PLS file");
		return FALSE;
	}

	if (g_ascii_strncasecmp (lines[current], "PlaylistName", 12) == 0) {
		current++;
	}

	if (g_ascii_strncasecmp (lines[current], "numberofentries", 15) == 0) {
		current++;
	}

	plspath = xmms_util_decode_path (xmms_transport_url_get (transport));

	for (; lines[current] != NULL; current += 3) {
		gchar *file, *title, *length, *url;
		xmms_playlist_entry_t *entry = NULL;

		file = get_value (lines[current], "File");

		if (!file) {
			break;
		}

		title = get_value (lines[current + 1], "Title");
		if (!title) {
			g_free (file);
			break;
		}

		length = get_value (lines[current + 2], "Length");
		if (!length) {
			g_free (file);
			g_free (title);
			break;
		}

		url = build_encoded_url (plspath, file);
		entry = xmms_playlist_entry_new (url);

		xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, length);
		xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE, title);
		xmms_playlist_add (playlist, entry);

		g_free (url);
		g_free (file);
		g_free (title);
		g_free (length);
		xmms_object_unref (entry);
	}

	g_free (plspath);
	g_strfreev (lines);
	return TRUE;
}

static gboolean
xmms_pls_write_playlist (xmms_playlist_plugin_t *plsplugin,
                         xmms_playlist_t *playlist,
                         gchar *filename)
{
	FILE *fp;
	gint current;
	GList *list, *node;
	xmms_error_t err;

	g_return_val_if_fail (plsplugin, FALSE);
	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (filename, FALSE);

	fp = fopen (filename, "w+");

	if (!fp) {
		return FALSE;
	}

	list = xmms_playlist_list (playlist, &err);

	if (!list) {
		return FALSE;
	}

	fprintf (fp, "[playlist]\n");

	for (current = 1, node = list; node != NULL; node = g_list_next (node)) {
		xmms_playlist_entry_t *entry = xmms_playlist_get_byid (playlist, (guint) node->data);
		const gchar *url, *title, *duration;
		gchar *dec;

		duration = xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION);
		title = xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE);
		url = xmms_playlist_entry_url_get (entry);
		dec = xmms_util_decode_path (url);

		fprintf (fp, "File%u=%s\n", current, dec);
		fprintf (fp, "Title%u=%s\n", current, title);
		if (!duration) {
			fprintf (fp, "Length%u=%s\n", current, "-1");
		} else {
			fprintf (fp, "Length%u=%s\n", current, duration);
		}

		xmms_object_unref (entry);
		g_free (dec);
		current++;
	}

	fprintf (fp, "NumberOfEntries=%u\n", current - 1);
	fprintf (fp, "Version=2\n");
	fclose (fp);

	g_list_free (list);
	return TRUE;
}

static gchar *
get_value (const gchar *pair, const gchar *key)
{
	gchar **split;
	gchar *ret;

	g_return_val_if_fail (pair, NULL);
	g_return_val_if_fail (key, NULL);

	split = g_strsplit (pair, "=", 2);
	g_return_val_if_fail (split, NULL);

	if (g_ascii_strncasecmp (split[0], key, strlen (key)) != 0) {
		g_strfreev (split);
		return NULL;
	}

	if (split[1] == NULL) {
		g_strfreev (split);
		return NULL;
	}

	ret = g_strstrip (g_strdup (split[1]));

	g_strfreev (split);
	return ret;
}

static gchar *
path_get_body (const gchar *path)
{
	gchar *beg, *end;

	g_return_val_if_fail (path, NULL);

	beg = strstr (path, "://");

	if (!beg) {
		return g_strndup (path, strcspn (path, "/"));
	}

	beg += 3;
	end = strchr (beg, '/');

	if (!end) {
		return g_strdup (path);
	}

	return g_strndup (path, end - path);
}

static gchar *
build_encoded_url (const gchar *plspath, const gchar *file)
{
	gchar *url;
	gchar *enc;
	gchar *path;

	g_return_val_if_fail (plspath, NULL);
	g_return_val_if_fail (file, NULL);

	if (strstr (file, "://") != NULL) {
		return g_strdup (file);
	}

	if (file[0] == '/') {

		path = path_get_body (plspath);
		url = g_build_filename (path, file, NULL);

		g_free (path);
		return url;
	}

	path = g_path_get_dirname (plspath);
	url = g_build_filename (path, file, NULL);

	enc = xmms_util_encode_path (url);
	g_free (url);
	g_free (path);
	return enc;
}
