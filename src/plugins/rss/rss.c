/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>

static gboolean xmms_rss_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_rss_init (xmms_xform_t *xform);
static gboolean xmms_rss_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);
static void xmms_rss_destroy (xmms_xform_t *xform);

XMMS_XFORM_PLUGIN("rss",
				  "reader for rss podcasts",
				  XMMS_VERSION,
				  "reader for rss podcasts",
				  xmms_rss_plugin_setup);

static gboolean
xmms_rss_plugin_setup (xmms_xform_plugin_t *xform_plugin) {
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT(methods);
	methods.init = xmms_rss_init;
	methods.browse = xmms_rss_browse;
	methods.destroy = xmms_rss_destroy;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
								  XMMS_STREAM_TYPE_MIMETYPE,
								  "application/xml",
								  NULL);

	xmms_magic_add ("xml header", "application/xml",
					"0 string <?xml", NULL);

	xmms_magic_extension_add ("application/xml", "*.xml");
	xmms_magic_extension_add ("application/xml", "*.rss");

	return TRUE;
}

static gboolean
xmms_rss_init (xmms_xform_t *xform) {
	xmms_xform_outdata_type_add (xform,
								 XMMS_STREAM_TYPE_MIMETYPE,
								 "application/x-xmms2-playlist-entries",
								 XMMS_STREAM_TYPE_END);

	return TRUE;
}

static void
xmms_rss_start_element (GMarkupParseContext *context, const gchar *elem_name,
						   const gchar **attr_names, const gchar **attr_values,
						   gpointer udata, GError **error) {
	int i;

	if (g_ascii_strncasecmp (elem_name, "enclosure", 9) != 0)
		return;

	if (!attr_names || !attr_values || !udata)
		return;

	for (i = 0; attr_names[i] && attr_values[i]; i++) {
		if (g_ascii_strncasecmp (attr_names[i], "url", 3) == 0) {
			xmms_xform_t *xform = (xmms_xform_t *)udata;

			xmms_xform_browse_add_entry (xform, attr_values[i], 0);
			xmms_xform_browse_add_entry_symlink (xform, attr_values[i], 0, NULL);
		}
	}
}

static gboolean
xmms_rss_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error) {
	int ret;
	gchar buffer[1024];
	GMarkupParseContext *ctx;
	GMarkupParser parser;

	g_return_val_if_fail (xform, FALSE);

	memset (&parser, 0, sizeof (GMarkupParser));

	parser.start_element = xmms_rss_start_element;
	ctx = g_markup_parse_context_new (&parser, 0, xform, NULL);

	xmms_error_reset (error);

	do {
		ret = xmms_xform_read (xform, buffer, 1024, error);
		if (!g_markup_parse_context_parse (ctx, buffer, ret, NULL))
			break;
	} while (ret > 0);

	g_markup_parse_context_free (ctx);
	xmms_error_reset (error);

	return TRUE;
}

static void xmms_rss_destroy (xmms_xform_t *xform) {
}
