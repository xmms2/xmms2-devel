/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_streamtype.h"
#include "xmmspriv/xmms_sample.h"
#include "xmmspriv/xmms_xform.h"
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
	xmms_stream_type_t *intype;
	xmms_stream_type_t *to;
	const GList *goal_hints;

	intype = xmms_xform_intype_get (xform);
	goal_hints = xmms_xform_goal_hints_get (xform);

	to = xmms_stream_type_coerce (intype, goal_hints);
	if (!to) {
		return FALSE;
	}

	conv = xmms_sample_converter_init (intype, to);
	if (!conv) {
		return FALSE;
	}

	xmms_xform_outdata_type_set (xform, to);
	xmms_object_unref (to);

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

static gint64
xmms_converter_plugin_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_conv_xform_data_t *data;
	gint64 res;
	gint64 scaled_samples;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	scaled_samples = xmms_sample_convert_scale (data->conv, samples);

	res = xmms_xform_seek (xform, scaled_samples, XMMS_XFORM_SEEK_SET, err);
	if (res == -1) {
		return -1;
	}

	scaled_samples = xmms_sample_convert_rev_scale (data->conv, res);

	xmms_sample_convert_reset (data->conv);

	return scaled_samples;
}

static gboolean
xmms_converter_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_converter_plugin_init;
	methods.destroy = xmms_converter_plugin_destroy;
	methods.read = xmms_converter_plugin_read;
	methods.seek = xmms_converter_plugin_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	 * Handle any pcm data...
	 * Well, we don't really..
	 */
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_PRIORITY,
				      100,
	                              XMMS_STREAM_TYPE_NAME,
				      "generic-pcmdata",
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

XMMS_XFORM_BUILTIN (converter,
                    "Sample format converter",
                    XMMS_VERSION,
                    "Sample format converter",
                    xmms_converter_plugin_setup);
