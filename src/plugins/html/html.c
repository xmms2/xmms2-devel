/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003  Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include <glib.h>
#include <glib/gprintf.h>

/*
 * Function prototypes
 */

static gboolean xmms_html_can_handle (const gchar *mime);
static gboolean xmms_html_read_playlist (xmms_playlist_plugin_t *plsplugin,
                                         xmms_transport_t *transport,
                                         xmms_playlist_t *playlist);

static gboolean valid_suffix (gchar **suffix, gchar *path);
static gchar* parse_tag (const gchar *tag, const gchar *plspath);
static gchar* build_url (const gchar *plspath, const gchar *file);
static gchar* path_get_body (const gchar *path);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_PLAYLIST, "html",
	                          "HTML Playlist " XMMS_VERSION,
	                          "HTML Playlist reader");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_html_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST, xmms_html_read_playlist);

	xmms_plugin_config_value_register (plugin, "suffixes", "mp3,ogg,flac,wav,spx,sid", NULL, NULL);

	return plugin;
}

static gboolean
xmms_html_can_handle (const gchar *mime)
{
	g_return_val_if_fail (mime, FALSE);

	XMMS_DBG ("xmms_html_can_handle (%s)", mime);

	if ((g_strncasecmp (mime, "text/html", 9) == 0))
		return TRUE;

	return FALSE;
}

static gboolean
xmms_html_read_playlist (xmms_playlist_plugin_t *plsplugin,
                         xmms_transport_t *transport,
                         xmms_playlist_t *playlist)
{
	gchar *buffer, *plsurl;
	gchar **tags;
	gchar **suffix;
	xmms_config_value_t *val;

	gint cnt, readlen, buflen;

	g_return_val_if_fail (plsplugin, FALSE);
	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (playlist, FALSE);

	buflen = xmms_transport_size (transport);
	if (buflen == 0) {
		XMMS_DBG ("Empty playlist, nothing to add here");
		return TRUE;
	}

	if (buflen == -1) {
		buflen = 4096;
	}

	buffer = g_malloc0 (buflen);
	g_return_val_if_fail (buffer, FALSE);

	readlen = 0;
	while (readlen < buflen) {
		xmms_error_t error;
		gint ret;

		ret = xmms_transport_read (transport, buffer + readlen, buflen - readlen, &error);

		XMMS_DBG ("Got %d bytes", ret);
		if (ret <= 0) {

			if (readlen > 0) {
				break;
			}

			g_free (buffer);
			return FALSE;
		}

		readlen += ret;
		g_assert (readlen >= 0);
	}

	tags = g_strsplit (buffer, "<", 0);
	g_free (buffer);

	val = xmms_plugin_config_lookup (plsplugin->plugin, "suffixes");
	suffix = g_strsplit (xmms_config_value_string_get (val), ",", 0);

	plsurl = xmms_util_decode_path (xmms_transport_url_get (transport));

	for (cnt = 0; tags[cnt] != NULL; cnt++) {
		gchar *url, *full, *enc;
		xmms_playlist_entry_t *entry = NULL;

		url = parse_tag (tags[cnt], plsurl);
		if (!url) {
			continue;
		}

		if (!valid_suffix (suffix, url)) {
			g_free (url);
			continue;
		}

		full = build_url (plsurl, url);

		enc = xmms_util_encode_path (full);
		entry = xmms_playlist_entry_new (enc);
		XMMS_DBG ("Adding %s", xmms_playlist_entry_url_get (entry));
		xmms_playlist_add (playlist, entry);

		g_free (enc);
		g_free (url);
		g_free (full);
	}

	g_free (plsurl);
	g_strfreev (suffix);
	g_strfreev (tags);

	return TRUE;
}

static gboolean
valid_suffix (gchar **suffix, gchar *path)
{
	guint current;

	g_return_val_if_fail (suffix, FALSE);
	g_return_val_if_fail (path, FALSE);

	for (current = 0; suffix[current] != NULL; current++) {
		if (g_str_has_suffix (path, g_strstrip (suffix[current])))
			return TRUE;
	}

	return FALSE;
}

static gchar *
build_url (const gchar *plspath, const gchar *file) {
	gchar *url;
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

	g_free (path);
	return url;
}

/*
 * proto://domain/dir/file -> proto://domain
 * domain/dir/file -> domain
 **/

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
parse_tag (const gchar *tag, const gchar *plspath)
{
	size_t urlend;
	gchar *href, *url;

	g_return_val_if_fail (tag, NULL);
	g_return_val_if_fail (plspath, NULL);

	if (g_ascii_strncasecmp (tag, "a ", 2) != 0) {
		return NULL;
	}

	href = strstr (tag, "href=\"");

	if (!href) {
		href = strstr (tag, "HREF=\"");
	}

	if (!href) {
		return NULL;
	}

	urlend = strcspn (href + 6, "\"");

	url = g_strndup (href + 6, urlend);

	return url;
}

