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

#include <glib.h>

#include "xmms/xmms_defs.h"
#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_streamtype.h"
#include "xmmspriv/xmms_sample.h"
#include "xmms/xmms_medialib.h"

#include <string.h>

typedef struct xmms_conv_xform_data_St {
	xmms_sample_converter_t *conv;
	void *outbuf;
	guint outlen;
} xmms_conv_xform_data_t;

static xmms_xform_plugin_t *converter_plugin;

static gboolean
xmms_converter_plugin_init (xmms_xform_t *xform)
{
	xmms_conv_xform_data_t *data;
	xmms_sample_converter_t *conv;
	const xmms_stream_type_t *intype;
	const xmms_stream_type_t *to;
	const GList *goal_hints;

	intype = xmms_xform_intype_get (xform);
	goal_hints = xmms_xform_goal_hints_get (xform);

	conv = xmms_sample_audioformats_coerce (intype, goal_hints);
	if (!conv) {
		return FALSE;
	}

	to = xmms_sample_converter_get_to (conv);
	xmms_xform_outdata_type_set (xform, to);

	data = g_new0 (xmms_conv_xform_data_t, 1);
	data->conv = conv;

	xmms_xform_private_data_set (xform, data);

	return TRUE;
}

static void
xmms_converter_plugin_destroy (xmms_xform_t *xform)
{
	xmms_conv_xform_data_t *data;
	
	data = xmms_xform_private_data_get (xform);

	if (data) {
		if (data->conv) {
			xmms_object_unref (data->conv);
		}
		
		g_free (data);
	}
}

static gint
xmms_converter_plugin_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error)
{
	xmms_conv_xform_data_t *data;
	char buf[1024];

	data = xmms_xform_private_data_get (xform);

	if (!data->outlen) {
		int r = xmms_xform_read (xform, buf, sizeof (buf), error);
		if (r <= 0) {
			return r;
		}
		xmms_sample_convert (data->conv, buf, r, &data->outbuf, &data->outlen);
	}

	len = MIN (len, data->outlen);
	memcpy (buffer, data->outbuf, len);
	data->outlen -= len;
	data->outbuf += len;

	return len;
}


gboolean
xmms_converter_plugin_add (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT(methods);
	methods.init = xmms_converter_plugin_init;
	methods.destroy = xmms_converter_plugin_destroy;
	methods.read = xmms_converter_plugin_read;
	/*
	  methods.seek
	  methods.get_mediainfo
	*/

	xmms_xform_plugin_setup (xform_plugin,
				 "converter",
				 "Converter " XMMS_VERSION,
				 "Sample format converter",
				 &methods);


	/*
	 * Handle any pcm data...
	 * Well, we don't really..
	 */
	xmms_xform_plugin_indata_add (xform_plugin,
				      XMMS_STREAM_TYPE_MIMETYPE,
				      "audio/pcm",
				      XMMS_STREAM_TYPE_END);

	converter_plugin = xform_plugin;
	return TRUE;
}

/*
xmms_xform_t *
xmms_converter_new (xmms_xform_t *prev, xmms_medialib_entry_t entry, GList *gt)
{
	xmms_xform_t *xform;

	xform = xmms_xform_new (converter_plugin, prev, entry, gt);

	return xform;
}
*/
