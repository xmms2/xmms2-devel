/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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

/*
 * Function prototypes
 */

static gboolean xmms_asx_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_asx_browse (xmms_xform_t *xform, const gchar *url,
                                 xmms_error_t *error);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN_DEFINE ("asx", "ASX reader", XMMS_VERSION,
                          "Playlist plugin for Advanced Stream Redirector files.",
                          xmms_asx_plugin_setup);

static gboolean
xmms_asx_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.browse = xmms_asx_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-asx-playlist",
	                              XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_set_out_stream_type (xform_plugin,
	                                       XMMS_STREAM_TYPE_MIMETYPE,
	                                       "application/x-xmms2-playlist-entries",
	                                       XMMS_STREAM_TYPE_END);

	xmms_magic_extension_add ("application/x-asx-playlist", "*.asx");

	xmms_magic_add ("ASX header", "application/x-asx-playlist",
	                "0 string/c <asx version=\"3.0\">", NULL);

	return TRUE;
}



/*
 * Member functions
 */

static void
xmms_asx_handle_ref (GMarkupParseContext *context, const gchar *elem_name,
                     const gchar **attr_names, const gchar **attr_vals,
                     gpointer udata, GError **error)
{
	xmms_xform_t *xform = (xmms_xform_t *) udata;
	gint i;

	if (g_ascii_strncasecmp (elem_name, "ref", 3) != 0) {
		return;
	}

	if (!attr_names || !attr_vals || !xform)  {
		return;
	}

	for (i = 0; attr_names[i] && attr_vals[i]; i++) {
		if (g_ascii_strncasecmp (attr_names[i], "href", 4) == 0) {
			xmms_xform_browse_add_symlink (xform, NULL, attr_vals[i]);
		}
	}
}


static gboolean
xmms_asx_browse (xmms_xform_t *xform, const char *url, xmms_error_t *error)
{
	gchar buff[XMMS_XFORM_MAX_LINE_SIZE];
	GMarkupParseContext *ctx;
	GMarkupParser parser;

	g_return_val_if_fail (xform, FALSE);

	memset (&parser, 0, sizeof (GMarkupParser));

	parser.start_element = xmms_asx_handle_ref;
	ctx = g_markup_parse_context_new (&parser, 0, xform, NULL);

	xmms_error_reset (error);

	while (xmms_xform_read_line (xform, buff, error)) {
		if (!g_markup_parse_context_parse (ctx, buff, strlen (buff), NULL)) {
			break;
		}
	}

	g_markup_parse_context_free (ctx);
	xmms_error_reset (error);

	return TRUE;
}
