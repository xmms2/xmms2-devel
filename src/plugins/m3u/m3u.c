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

static gboolean xmms_m3u_can_handle (const gchar *mimetype);
static gboolean xmms_m3u_read_playlist (xmms_transport_t *transport, guint playlist_id);
/* hahahaha ... g string */
static GString *xmms_m3u_write_playlist (guint32 *list);

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

static xmms_medialib_entry_t
parse_line (const gchar *line, const gchar *m3u_path)
{
	xmms_medialib_entry_t entry;
	gchar newp[1024], *p;

	g_assert (line);

	/* used to determine whether we got a valid path/url, see below */
	newp[0] = '\0';

	/* check for an absolute path */
	if (line[0] == '/') {
		g_snprintf (newp, sizeof (newp), "file://%s", line);
	} else {
		/* check whether it's an url (proto://...) */
		p = strchr (line, ':');

		if (p && p[1] == '/' && p[2] == '/') {
			g_snprintf (newp, sizeof (newp), "%s", line);
		} else {
			/* get the directory from the path to the m3u file.
			 * since we must not mess with the original path, get our
			 * own copy now:
			 */
			m3u_path = g_strdup (m3u_path);

			p = strrchr (m3u_path, '/');
			g_assert (p); /* m3u_path is always an encoded path */
			*p = '\0';

			g_snprintf (newp, sizeof (newp), "%s/%s",
			            m3u_path, line);
			g_free ((gchar *) m3u_path); /* free our copy */
		}
	}

	/* in all code paths, newp should have been written */
	g_assert (newp[0]);

	entry = xmms_medialib_entry_new (newp);

	return entry;
}

static gboolean
xmms_m3u_read_playlist (xmms_transport_t *transport, guint playlist_id)
{
	gchar line[XMMS_TRANSPORT_MAX_LINE_SIZE];
	xmms_error_t err;
	gboolean extm3u = FALSE;

	g_return_val_if_fail (transport, FALSE);

	xmms_error_reset (&err);

	if (!xmms_transport_read_line (transport, line, &err)) {
		XMMS_DBG ("Error reading m3u-file");
		return FALSE;
	}

	if (strcmp (line, "#EXTM3U") == 0) {
		extm3u = TRUE;
		if (!xmms_transport_read_line (transport, line, &err)) {
			return FALSE;
		}
	}

	do {
		xmms_medialib_entry_t entry;
		gchar *title = NULL, *duration = NULL;

		if (extm3u && line[0] == '#') {
			gchar *p;
			int read, write;

			p = strchr (line, ',');
			if (p) {
				*p = '\0';
				*p++;
			} else {
				xmms_log_error ("Malformated m3u");
				return FALSE;
			}

			/** @todo 
			 *  check whether the data we read is actually
			 *  ISO-8859-1, might be anything else as well.
			 */
			title = g_convert (p, strlen (p), "UTF-8", "ISO-8859-1",
			                   &read, &write, NULL);
			duration = g_strdup (line + 8);



			if (!xmms_transport_read_line (transport, line, &err)) {
				return FALSE;
			}
		}

		entry = parse_line (line,
				    xmms_transport_url_get (transport));

		if (title) {
			xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, title);
			g_free (title);
		}
		if (duration) {
			xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			                                  duration);
			g_free (duration);
		}

		xmms_medialib_playlist_add (playlist_id, entry);

	} while (xmms_transport_read_line (transport, line, &err));

	return TRUE;
}

static GString *
xmms_m3u_write_playlist (guint32 *list)
{
	xmms_error_t err;
	gint i = 0;
	GString *ret;

	g_return_val_if_fail (list, FALSE);

	xmms_error_reset (&err);

	ret = g_string_new ("#EXTM3U\n");

	while (list[i]) {
		xmms_medialib_entry_t entry = list[i];
		gchar *url;
		gchar *artist, *title;
		gint duration = 0;

		duration = xmms_medialib_entry_property_get_int (entry,
				XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION);

		artist = xmms_medialib_entry_property_get (entry,
				XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST);

		title = xmms_medialib_entry_property_get (entry,
				XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE);

		if (title && artist && duration) {
			g_string_append_printf (ret, "#EXTINF:%d,%s - %s\n", 
						duration / 1000, artist, title);
			g_free (artist);
			g_free (title);
		}

		url = xmms_medialib_entry_property_get (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_URL);
		g_assert (url);

		if (g_strncasecmp (url, "file://", 7) == 0) {
			g_string_append_printf (ret, "%s\n", url+7);
		} else {
			g_string_append_printf (ret, "%s\n", url);
		}
		g_free (url);

		i++;
	}

	return ret;
}
