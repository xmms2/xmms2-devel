/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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


/** @file
 *
 */

#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_output.h"
#include "xmmspriv/xmms_visualization.h"
#include "xmms/xmms_log.h"

#include <glib.h>
#include <stdio.h>
#include <limits.h>

#include "common.h"

static gboolean xmms_vis_init (xmms_xform_t *xform);
static void xmms_vis_destroy (xmms_xform_t *xform);
static gint xmms_vis_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                          xmms_error_t *error);
static gint64 xmms_vis_seek (xmms_xform_t *xform, gint64 offset,
                            xmms_xform_seek_mode_t whence, xmms_error_t *err);

static gboolean
xmms_vis_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_vis_init;
	methods.destroy = xmms_vis_destroy;
	methods.read = xmms_vis_read;
	methods.seek = xmms_vis_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/pcm",
	                              XMMS_STREAM_TYPE_FMT_FORMAT,
	                              XMMS_SAMPLE_FORMAT_S16,
	                              XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                              44100,
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}


static gboolean
xmms_vis_init (xmms_xform_t *xform)
{
	gint srate;

	g_return_val_if_fail (xform, FALSE);

	srate = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	/* yeah, baby! ask mr. output & calculate your stuff here */

	xmms_xform_outdata_type_copy (xform);

	XMMS_DBG ("Visualization hook initialized successfully!");

	return TRUE;
}

static void
xmms_vis_destroy (xmms_xform_t *xform)
{
	g_return_if_fail (xform);
}

static gint
xmms_vis_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
              xmms_error_t *error)
{
	gint read, chan;

	g_return_val_if_fail (xform, -1);

	chan = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_CHANNELS);

	/* perhaps rework this later */
	if (len > XMMSC_VISUALIZATION_WINDOW_SIZE * chan * sizeof (short)) {
		len = XMMSC_VISUALIZATION_WINDOW_SIZE * chan * sizeof (short);
	}

	read = xmms_xform_read (xform, buf, len, error);
	if (read > 0) {
		send_data (chan, read / sizeof (short), buf);
	}

	return read;
}

static gint64
xmms_vis_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	return xmms_xform_seek (xform, offset, whence, err);
}


XMMS_XFORM_BUILTIN (visualization,
                    "visualization hook",
                    XMMS_VERSION,
                    "visualization hook",
                    xmms_vis_plugin_setup);

/** @} */
