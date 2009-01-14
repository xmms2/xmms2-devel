/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>
#include <libxml/xmlreader.h>

typedef struct xmms_rss_data_St {
	xmms_xform_t *xform;
	xmms_error_t *error;
	gboolean parse_failure;
} xmms_rss_data_t;

static gboolean xmms_rss_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_rss_init (xmms_xform_t *xform);
static gboolean xmms_rss_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);

XMMS_XFORM_PLUGIN ("rss",
                   "reader for rss podcasts",
                   XMMS_VERSION,
                   "reader for rss podcasts",
                   xmms_rss_plugin_setup);

static gboolean
xmms_rss_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_rss_init;
	methods.browse = xmms_rss_browse;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-xmms2-xml+rss",
	                              NULL);

	xmms_magic_extension_add ("application/xml", "*.rss");

	return TRUE;
}

static gboolean
xmms_rss_init (xmms_xform_t *xform)
{
	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/x-xmms2-playlist-entries",
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static void
xmms_rss_start_element (xmms_rss_data_t *data, const xmlChar *name,
                        const xmlChar **attrs)
{
	xmms_xform_t *xform = data->xform;
	int i;

	XMMS_DBG ("start elem %s", name);

	if (!attrs || !data)
		return;

	if (!xmlStrEqual (name, BAD_CAST "enclosure"))
		return;

	for (i = 0; attrs[i]; i += 2) {
		char *attr;

		if (!xmlStrEqual (attrs[i], BAD_CAST "url"))
			continue;

		attr = (char *) attrs[i + 1];

		XMMS_DBG ("Found %s", attr);
		xmms_xform_browse_add_symlink (xform, NULL, attr);

		break;
	}
}

static void
xmms_rss_error (xmms_rss_data_t *data, const char *msg, ...)
{
	va_list ap;
	char str[1000];

	va_start (ap, msg);
	vsnprintf (str, sizeof (str), msg, ap);
	va_end (ap);

	data->parse_failure = TRUE;
	xmms_error_set (data->error, XMMS_ERROR_INVAL, str);
}

static gboolean
xmms_rss_browse (xmms_xform_t *xform, const gchar *url, xmms_error_t *error)
{
	int ret;
	char buffer[1024];
	xmlSAXHandler handler;
	xmlParserCtxtPtr ctx;
	xmms_rss_data_t data;

	g_return_val_if_fail (xform, FALSE);

	memset (&handler, 0, sizeof (handler));
	memset (&data, 0, sizeof (data));

	handler.startElement = (startElementSAXFunc) xmms_rss_start_element;
	handler.error = (errorSAXFunc) xmms_rss_error;
	handler.fatalError = (fatalErrorSAXFunc) xmms_rss_error;

	data.xform = xform;
	data.error = error;
	data.parse_failure = FALSE;

	xmms_error_reset (error);

	ctx = xmlCreatePushParserCtxt (&handler, &data, buffer, 0, NULL);
	if (!ctx) {
		xmms_error_set (error, XMMS_ERROR_OOM,
		                "Could not allocate xml parser");
		return FALSE;
	}

	while ((ret = xmms_xform_read (xform, buffer, sizeof (buffer), error)) > 0) {
		xmlParseChunk (ctx, buffer, ret, 0);
	}

	if (ret < 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "xmms_xform_read failed");
		return FALSE;
	}

	if (data.parse_failure)
		return FALSE;

	xmlParseChunk (ctx, buffer, 0, 1);

	xmms_error_reset (error);
	xmlFreeParserCtxt (ctx);

	return TRUE;
}
