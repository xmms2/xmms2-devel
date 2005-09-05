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




#include "xmms/xmms_defs.h"
#include "xmms/xmms_plugin.h"
#include "xmms/xmms_transport.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_plsplugins.h"
#include "xmms/xmms_medialib.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Type definitions
 */

/*
 * Function prototypes
 */


static gboolean xmms_pls_read_playlist (xmms_transport_t *transport, guint playlist_id);
static GString *xmms_pls_write_playlist (guint32 *list);

static gchar *build_encoded_url (const gchar *plspath, const gchar *file);
static gchar *path_get_body (const gchar *path);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_PLAYLIST, 
				  XMMS_PLAYLIST_PLUGIN_API_VERSION,
				  "pls",
				  "PLS Playlist " XMMS_VERSION,
				  "PLS Playlist reader / writer");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST, xmms_pls_read_playlist);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE_PLAYLIST, xmms_pls_write_playlist);

	xmms_plugin_magic_add (plugin, "pls header", "audio/x-scpls",
	                       "0 string [playlist]", NULL);

	return plugin;
}

/*
 * Member functions
 */

typedef struct {
	gint num;
	gchar *file;
	gchar *title;
	gchar *length;
} xmms_pls_entry_t;

static void
xmms_pls_add_entry (const gchar *plspath, guint playlist_id, xmms_pls_entry_t *e)
{
	if (e->file) {
		xmms_medialib_entry_t entry;
		gchar *url;

		url = build_encoded_url (plspath, e->file);
		entry = xmms_medialib_entry_new (url);
		g_free (url);
		
		if (e->title)
			xmms_medialib_entry_property_set_str (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, e->title);

		if (e->length && atoi (e->length) > 0)
			xmms_medialib_entry_property_set_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION, 
							      atoi (e->length));

		xmms_medialib_playlist_add (playlist_id, entry);

		g_free (e->file);
		e->file = NULL;
	}
	if (e->title) {
		g_free (e->title);
		e->title = NULL;
	}
	if (e->length) {
		g_free (e->length);
		e->length = NULL;
	}
}

static gboolean
xmms_pls_read_playlist (xmms_transport_t *transport, guint playlist_id)
{
	gchar buffer[XMMS_TRANSPORT_MAX_LINE_SIZE];
	gint num = -1;
	gchar **val;
	const gchar *plspath;
	xmms_pls_entry_t entry;
	xmms_error_t err;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (playlist_id,  FALSE);

	xmms_error_reset (&err);

	plspath = xmms_transport_url_get (transport);

	if (!xmms_transport_read_line (transport, buffer, &err)) {
		XMMS_DBG ("Error reading pls-file");
		return FALSE;
	}

	/* for completeness' sake, check for the pls header here again, too
	 * (it's already done in the magic check)
	 */
	if (g_ascii_strncasecmp (buffer, "[playlist]", 10) != 0) {
		XMMS_DBG ("Not a PLS file");
		return FALSE;
	}

	memset (&entry, 0, sizeof (entry));
	entry.num=-1;

	while (xmms_transport_read_line (transport, buffer, &err)) {
		gchar *np, *ep;

		if (g_ascii_strncasecmp (buffer, "File", 4) == 0) {
			np = &buffer[4];
			val = &entry.file;
		} else if (g_ascii_strncasecmp (buffer, "Length", 6) == 0) {
			np = &buffer[6];
			val = &entry.length;
		} else if (g_ascii_strncasecmp (buffer, "Title", 5) == 0) {
			np = &buffer[5];
			val = &entry.title;
		} else {
			continue;
		}

		num = strtol (np, &ep, 10);
		if (!ep || *ep != '=') {
			XMMS_DBG ("Broken line '%s', skipping", buffer);
			continue;
		}

		if (entry.num != num && entry.num != -1) {
			xmms_pls_add_entry (plspath, playlist_id, &entry);
		}

		*val = g_strdup (ep + 1);
		entry.num = num;
	}

	xmms_pls_add_entry (plspath, playlist_id, &entry);

	return TRUE;
}

static GString *
xmms_pls_write_playlist (guint32 *list)
{
	gint current;
	GString *ret;
	gint i = 0;

	g_return_val_if_fail (list, FALSE);

	ret = g_string_new ("[playlist]\n");

	current = 1;
	while (list[i]) {
		xmms_medialib_entry_t entry = list[i];
		gint duration;
		const gchar *title;
		gchar *url;

		duration = xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION);
		title = xmms_medialib_entry_property_get_str (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE);
		url = xmms_medialib_entry_property_get_str (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_URL);

		if (g_strncasecmp (url, "file://", 7) == 0) {
			g_string_append_printf (ret, "File%u=%s\n", current, url+7);
		} else {
			g_string_append_printf (ret, "File%u=%s\n", current, url);
		}
		g_string_append_printf (ret, "Title%u=%s\n", current, title);
		if (!duration) {
			g_string_append_printf (ret, "Length%u=%s\n", current, "-1");
		} else {
			g_string_append_printf (ret, "Length%u=%d\n", current, duration);
		}

		g_free (url);
		current++;
		i++;
	}

	g_string_append_printf (ret, "NumberOfEntries=%u\n", current - 1);
	g_string_append (ret, "Version=2\n");

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
	gchar *path;

	g_return_val_if_fail (plspath, NULL);
	g_return_val_if_fail (file, NULL);

	if (strstr (file, "://") != NULL) {
		return g_strdup (file);
	}

	if (file[0] == '/') {
		path = path_get_body (plspath);
	} else {
		path = g_path_get_dirname (plspath);
	}
	url = g_build_filename (path, file, NULL);

	g_free (path);
	return url;
}
