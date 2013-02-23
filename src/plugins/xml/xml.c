/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <ctype.h>

#define BUFFER_SIZE 4096

static gboolean xmms_xml_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_xml_init (xmms_xform_t *xform);
static gint xmms_xml_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static gint64 xmms_xml_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);

XMMS_XFORM_PLUGIN ("xml",
                   "XML plugin", XMMS_VERSION,
                   "XML plugin", xmms_xml_plugin_setup);

static gboolean
xmms_xml_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_xml_init;
	methods.read = xmms_xml_read;
	methods.seek = xmms_xml_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/xml",
	                              NULL);

	xmms_magic_add ("xml header", "application/xml",
	                "0 string <?xml", NULL);
	xmms_magic_add ("xml header", "application/xml",
	                "0 string \xef\xbb\xbf<?xml", NULL);

	return TRUE;
}

static gchar *
get_root_node_name (xmms_xform_t *xform)
{
	guint8 buf[BUFFER_SIZE];
	gint read, i, start, len;
	gchar *ret = NULL;
	xmms_error_t error;

	xmms_error_reset (&error);

	read = xmms_xform_peek (xform, buf, BUFFER_SIZE, &error);
	if (read < 1) {
		xmms_log_error ("Couldn't get data: %s", xmms_error_message_get (&error));
		return NULL;
	}

	start = -1;
	len = 0;
	for (i = 0; i < read; i++) {
		if (start < 0) {
			if (buf[i] == '<' && buf[i + 1] != '!' && buf[i + 1] != '?') {
				start = i;
			}
		} else {
			if (isalpha (buf[i])) {
				len++;
			} else if (len) {
				ret = g_malloc (len + 1);
				memcpy (ret, buf + start + 1, len);
				ret[len] = '\0';
				break;
			}
		}

	}

	return ret;
}

static gboolean
xmms_xml_init (xmms_xform_t *xform)
{
	gchar *mime, *root_node = NULL;

	root_node = get_root_node_name (xform);
	if (!root_node) {
		XMMS_DBG ("unable to find root node of xml document");
		return FALSE;
	}

	mime = g_strconcat ("application/x-xmms2-xml+", root_node, NULL);
	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             mime,
	                             XMMS_STREAM_TYPE_END);

	g_free (root_node);
	g_free (mime);
	return TRUE;
}

static gint
xmms_xml_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
               xmms_error_t *err)
{
	return xmms_xform_read (xform, buf, len, err);
}

static gint64
xmms_xml_seek (xmms_xform_t *xform, gint64 samples,
               xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	return xmms_xform_seek (xform, samples, whence, err);
}
