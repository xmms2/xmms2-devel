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

static gboolean xmms_m3u_can_handle (const gchar *mimetype);
static gboolean xmms_m3u_read_playlist (xmms_playlist_plugin_t *plsplugin, xmms_transport_t *transport,
		xmms_playlist_t *playlist);
static gboolean xmms_m3u_write_playlist (xmms_playlist_plugin_t *plsplugin,xmms_playlist_t *playlist,
		gchar *filename);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_PLAYLIST, "m3u",
			"M3U Playlist " XMMS_VERSION,
			"M3U Playlist reader / writer");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE,
				xmms_m3u_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ_PLAYLIST,
				xmms_m3u_read_playlist);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE_PLAYLIST,
				xmms_m3u_write_playlist);

	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_m3u_can_handle (const gchar *mime)
{
	g_return_val_if_fail (mime, FALSE);

	XMMS_DBG ("xmms_m3u_can_handle (%s)", mime);

	if ((g_strncasecmp (mime, "audio/mpegurl", 13) == 0))
		return TRUE;

	if ((g_strncasecmp (mime, "audio/x-mpegurl", 15) == 0))
		return TRUE;

	if ((g_strncasecmp (mime, "audio/m3u", 9) == 0))
		return TRUE;

	return FALSE;
}

static xmms_playlist_entry_t *
parse_line (const gchar *line, const gchar *m3u_path)
{
	xmms_playlist_entry_t *entry = NULL;
	gchar newp[XMMS_MAX_URI_LEN + 1], *encoded = NULL, *p;

	g_assert (line);

	/* used to determine whether we got a valid path/url, see below */
	newp[0] = '\0';

	/* check for an absolute path */
	if (line[0] == '/') {
		encoded = xmms_util_encode_path (line);
		g_snprintf (newp, sizeof (newp), "file://%s", encoded);
	} else {
		/* check whether it's an url (proto://...) */
		p = strchr (line, ':');

		if (p && p[1] == '/' && p[2] == '/') {
			/** @todo 
			 * how do we know whether the url is properly encoded?
			 * the problem is, if we encode an encoded path, all we'll
			 * get is junk :/
			 */
			encoded = xmms_util_encode_path (line);
			g_snprintf (newp, sizeof (newp), "%s", encoded);
		} else {
			/* get the directory from the path to the m3u file.
			 * since we must not mess with the original path, get our
			 * own copy now:
			 */
			m3u_path = g_strdup (m3u_path);

			p = strrchr (m3u_path, '/');
			g_assert (p); /* m3u_path is always an encoded path */
			*p = '\0';

			encoded = xmms_util_decode_path (line);
			g_snprintf (newp, sizeof (newp), "%s/%s",
			            m3u_path, encoded);
			g_free ((gchar *) m3u_path); /* free our copy */
		}
	}

	/* in all code paths, newp should have been written */
	g_assert (newp[0]);

	entry = xmms_playlist_entry_new (newp);
	g_free (encoded);

	return entry;
}

static gboolean
xmms_m3u_read_playlist (xmms_playlist_plugin_t *plsplugin, xmms_transport_t *transport, xmms_playlist_t *playlist)
{
	gint len = 0, buffer_len = 0;
	gchar **lines, **line, *buffer;
	xmms_error_t error;
	gboolean extm3u = FALSE;

	g_return_val_if_fail (plsplugin, FALSE); 
	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (playlist, FALSE);

	buffer_len = xmms_transport_size (transport);
	buffer = g_malloc0 (buffer_len + 1);

	while (len < buffer_len) {
		gint ret;

		ret = xmms_transport_read (transport, buffer + len, buffer_len, &error);
		XMMS_DBG ("Got %d bytes.", ret);

		if (ret <= 0) {
			g_free (buffer);
			return FALSE;
		}

		len += ret;
		g_assert (len >= 0);
	}

	buffer[len] = '\0';

	lines = g_strsplit (buffer, "\n", 0);
	g_free (buffer);

	if (!lines)
		return FALSE;

	line = lines;
	if (!line) {
		g_strfreev (lines);
		return FALSE;
	}

	if (strcmp (*line, "#EXTM3U") == 0) {
		extm3u = TRUE;
		line++;
	}

	while (*line) {
		xmms_playlist_entry_t *entry = NULL;

		if (!**line) {
			line++;
			continue;
		}

		if (extm3u && **line == '#') {
			gchar *title, *p, *duration;
			int read, write;

			p = strchr (*line, ',');
			if (p) {
				*p = '\0';
				*p++;
			} else {
				xmms_log_error ("Malformated m3u");
				g_strfreev (lines);
				return FALSE;
			}

			/** @todo 
			 *  check whether the data we read is actually
			 *  ISO-8859-1, might be anything else as well.
			 */
			title = g_convert (p, strlen (p), "UTF-8", "ISO-8859-1",
			                   &read, &write, NULL);
			duration = *line + 8;

			line++; /* skip to track */

			if (*line) {
				entry = parse_line (*line,
				                    xmms_transport_url_get (transport));
			}

			g_assert (entry);

			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION,
			                                  duration);
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE, title);
			g_free (title);
		} else {
			entry = parse_line (*line,
			                    xmms_transport_url_get (transport));
		}

		g_assert (entry);

		XMMS_DBG ("Adding %s", xmms_playlist_entry_url_get (entry));
		xmms_playlist_add (playlist, entry);

		if (*line)
			line++;
	}

	g_strfreev (lines);

	return TRUE;
}

static gboolean
xmms_m3u_write_playlist (xmms_playlist_plugin_t *plsplugin, xmms_playlist_t *playlist, gchar *filename)
{
	FILE *fp;
	GList *list, *l;
	xmms_error_t err;

	g_return_val_if_fail (plsplugin, FALSE); 
	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (filename, FALSE);

	xmms_error_reset (&err);

	fp = fopen (filename, "w");
	if (!fp)
		return FALSE;

	list = xmms_playlist_list (playlist, &err);

	fprintf (fp, "#EXTM3U\n");

	for (l = list; l; l = l->next) {
		xmms_playlist_entry_t *entry = xmms_playlist_get_byid (playlist, (guint) l->data);
		gchar *url;
		const gchar *d, *artist, *title;
		gint duration = 0;

		d = xmms_playlist_entry_property_get (entry,
				XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION);
		if (d)
			duration = atoi (d);

		artist = xmms_playlist_entry_property_get (entry,
				XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST);

		title = xmms_playlist_entry_property_get (entry,
				XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE);

		xmms_object_unref (entry);

		if (title && artist && duration) {
			fprintf (fp, "#EXTINF:%d,%s - %s\n", duration / 1000,
			         artist, title);
		}

		url = xmms_util_decode_path (xmms_playlist_entry_url_get (entry));
		g_assert (url);

		fprintf (fp, "%s\n", url);
		g_free (url);
	}

	fclose (fp);

	return TRUE;
}
