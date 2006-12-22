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
#include <libxml/xmlreader.h>

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
xmms_rss_start_element (void *udata, const xmlChar *name, const xmlChar **attrs) {
	int i;

	XMMS_DBG("start elem %s", name);

	if (xmlStrncmp (name, (xmlChar *)"enclosure", 9) != 0)
		return;

	if (!attrs || !udata)
		return;

	for (i = 0; attrs[i]; i += 2) {
		if (xmlStrncmp (attrs[i], (xmlChar *)"url", 3) == 0) {
			xmms_xform_t *xform = (xmms_xform_t *)udata;

			XMMS_DBG ("Found %s", attrs[i+1]);
			xmms_xform_browse_add_entry (xform, (char *)attrs[i+1], 0);
			xmms_xform_browse_add_entry_symlink (xform, (char *)attrs[i+1], 0, NULL);

			break;
		}
	}
}

static void
xmms_rss_error (void *udata, const char *msg, ...) {
	va_list ap;
	char str[1000];

	va_start (ap, msg);
	vsnprintf (str, 1000, msg, ap);
	va_end (ap);

	XMMS_DBG ("%s", str);
}

static gboolean
xmms_rss_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error) {
	int ret;
	char buffer[1024];
	xmlSAXHandler handler;
	xmlParserCtxtPtr ctx;

	g_return_val_if_fail (xform, FALSE);

	memset (&handler, '\0', sizeof (xmlSAXHandler));
	handler.startElement = xmms_rss_start_element;
	handler.error = xmms_rss_error;
	handler.fatalError = xmms_rss_error;
	handler.warning = xmms_rss_error;

	xmms_error_reset (error);

	ret = xmms_xform_read (xform, buffer, 1024, error);
	ctx = xmlCreatePushParserCtxt (&handler, xform, buffer, ret, NULL);

	while ((ret = xmms_xform_read (xform, buffer, 1024, error)) > 0) {
		XMMS_DBG("read: %d", ret);
		xmlParseChunk (ctx, buffer, ret, 0);
	}

	xmlParseChunk (ctx, buffer, 0, 1);

	xmms_error_reset (error);

	xmlFreeParserCtxt (ctx);

	return TRUE;
}

static void xmms_rss_destroy (xmms_xform_t *xform) {
}
