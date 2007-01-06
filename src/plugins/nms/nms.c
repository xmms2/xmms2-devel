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
#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#include <string.h>

#define BUILD_TARGET_ARM 1

#include "generic-nms.h"
#include "nmsplugin.h"
#include "plugin-internals.h"
/*
 * Type definitions
 */

#define SAMPLERATE 44100
#define CHANNELS 2


typedef struct xmms_nms_data_t {
	media_buf_t buf;
} xmms_nms_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_nms_plugin_setup (xmms_output_plugin_t *output_plugin);
static gboolean xmms_nms_new (xmms_output_t *output);
static void xmms_nms_destroy (xmms_output_t *output);
static void xmms_nms_flush (xmms_output_t *output);
static gboolean xmms_nms_open (xmms_output_t *output);
static void xmms_nms_close (xmms_output_t *output);
static void xmms_nms_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err);
static gboolean xmms_nms_format_set (xmms_output_t *output, const xmms_stream_type_t *format);

/*
 * Plugin header
 */
XMMS_OUTPUT_PLUGIN ("nms",
                    "NMS output",
                    XMMS_VERSION,
                    "Neuros output plugin",
                    xmms_nms_plugin_setup);

static gboolean
xmms_nms_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);
	methods.new = xmms_nms_new;
	methods.destroy = xmms_nms_destroy;
	methods.flush = xmms_nms_flush;

	methods.open = xmms_nms_open;
	methods.close = xmms_nms_close;
	methods.format_set = xmms_nms_format_set;
	methods.write = xmms_nms_write;

	xmms_output_plugin_methods_set (plugin, &methods);

	return TRUE;
}

static gboolean
xmms_nms_new (xmms_output_t *output)
{
	xmms_nms_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_nms_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_S16, CHANNELS, SAMPLERATE);
	xmms_output_private_data_set (output, data);

	return TRUE;
}

static void
xmms_nms_destroy (xmms_output_t *output)
{
	xmms_nms_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	g_free (data);
}

static void
xmms_nms_flush (xmms_output_t *output)
{
	/* not implemented */
}

static gboolean
xmms_nms_open (xmms_output_t *output)
{
	static media_desc_t desc = {
		.adesc = {
			.audio_type = NMS_AC_PCM,
			.num_channels = CHANNELS,
			.sample_rate = SAMPLERATE,
			.bitrate = CHANNELS * SAMPLERATE * 16,
		},
		.vdesc = {
			.video_type = NMS_VC_NO_VIDEO,
			.width = 0,
			.height = 0,
			.frame_rate = 0,
		},
		.sdesc = {
			.subtitle_type = NMS_SC_NO_SUBTITLE,
		},
	};

	XMMS_DBG ("Opening audio device");

	PluginLoad ();

	if (OutputSelect (NMS_PLUGIN_MULTIMEDIA)) {
		xmms_log_error ("Error selecting multimedia plugin");
		return FALSE;
	}


	if (OutputInit (&desc, 0)) {
		xmms_log_error ("Error opening output");
		return FALSE;
	}

	OutputStart ();

	return TRUE;
}

static void
xmms_nms_close (xmms_output_t *output)
{
	OutputFinish (1);
}

static void
xmms_nms_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err)
{
	xmms_nms_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	/* What is the size of buffer? */

	while (OutputGetBuffer (&data->buf, 1000, 0))
		;

	memcpy (data->buf.abuf.data, buffer, len);
	data->buf.abuf.size = len;
	data->buf.curbuf = &data->buf.abuf;

	OutputWrite (&data->buf);
}

static gboolean
xmms_nms_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	gint sformat, channels, srate;

	sformat = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_FORMAT);
	channels = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_CHANNELS);
	srate = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_SAMPLERATE);

	if (sformat != XMMS_SAMPLE_FORMAT_S16) {
		xmms_log_error ("Bad sample format!");
		return FALSE;
	}
	if (channels != CHANNELS) {
		xmms_log_error ("Bad number of channels!");
		return FALSE;
	}
	if (srate != SAMPLERATE) {
		xmms_log_error ("Bad samplerate!");
		return FALSE;
	}

	return TRUE;
}

