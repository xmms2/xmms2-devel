/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#include <xmms/xmms_xformplugin.h>
#include <xmmsc/xmmsc_util.h>
#include <xmms/xmms_util.h>
#include <xmms/xmms_log.h>

#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Type definitions
 */

/*
 * Function prototypes
 */

/*static gboolean xmms_pls_read_playlist (xmms_transport_t *transport, guint playlist_id);*/
static gboolean xmms_pls_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_pls_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN_DEFINE ("pls",
                          "PLS reader",
                          XMMS_VERSION,
                          "Playlist parser for PLS files.",
                          xmms_pls_plugin_setup);

static gboolean
xmms_pls_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.browse = xmms_pls_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-scpls",
	                              NULL);

	xmms_xform_plugin_set_out_stream_type (xform_plugin,
	                                       XMMS_STREAM_TYPE_MIMETYPE,
	                                       "application/x-xmms2-playlist-entries",
	                                       XMMS_STREAM_TYPE_END);

	xmms_magic_add ("pls header",
	                "audio/x-scpls",
	                "0 string [playlist]\r\n",
	                "0 string [playlist]\n", NULL);

	xmms_magic_extension_add ("audio/x-scpls", "*.pls");

	return TRUE;
}

/*
 * Member functions
 */

typedef struct {
	gint num;
	gchar *file;
	gchar *title;
} xmms_pls_entry_t;

static void
xmms_pls_add_entry (xmms_xform_t *xform,
                    const gchar *plspath,
                    xmms_pls_entry_t *e)
{
	if (e->file) {
		gchar *path;

		path = xmms_build_playlist_url (plspath, e->file);

		xmms_xform_browse_add_symlink (xform, NULL, path);
		if (e->title)
			xmms_xform_browse_add_entry_property_str (xform, "title", e->title);

		g_free (path);
		g_free (e->file);
		e->file = NULL;
	}

	if (e->title) {
		g_free (e->title);
		e->title = NULL;
	}
}

static gboolean
xmms_pls_browse (xmms_xform_t *xform, const char *url, xmms_error_t *error)
{
	gchar buffer[XMMS_XFORM_MAX_LINE_SIZE];
	gint num = -1;
	gchar **val;
	const gchar *plspath;
	xmms_pls_entry_t entry;

	g_return_val_if_fail (xform, FALSE);

	xmms_error_reset (error);

	plspath = xmms_xform_get_url (xform);

	if (!xmms_xform_read_line (xform, buffer, error)) {
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

	while (xmms_xform_read_line (xform, buffer, error)) {
		gchar *np, *ep;

		if (g_ascii_strncasecmp (buffer, "File", 4) == 0) {
			np = &buffer[4];
			val = &entry.file;
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

		ep++; /* Skip the '=' */

		/* Remove leading and trailing whitespace from the value. */
		g_strstrip (ep);

		/* Ignore empty values. */
		if (!*ep) {
			XMMS_DBG ("Ignoring empty value in line '%s'", buffer);
			continue;
		}

		if (entry.num != num && entry.num != -1) {
			xmms_pls_add_entry (xform, plspath, &entry);
		}

		*val = g_strdup (ep);
		entry.num = num;
	}

	xmms_pls_add_entry (xform, plspath, &entry);

	return TRUE;
}
