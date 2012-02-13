/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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

#include "xmms/xmms_outputplugin.h"

#include <glib.h>

/*
 * Type definitions
 */
typedef struct xmms_null_data_St {
	gint us_per_byte;
} xmms_null_data_t;

/*
 * Function prototypes
 */
static gboolean xmms_null_plugin_setup (xmms_output_plugin_t *plugin);
static void xmms_null_flush (xmms_output_t *output);
static void xmms_null_write (xmms_output_t *output, gpointer buffer, gint len,
                             xmms_error_t *error);
static gboolean xmms_null_open (xmms_output_t *output);
static void xmms_null_close (xmms_output_t *output);
static gboolean xmms_null_new (xmms_output_t *output);
static void xmms_null_destroy (xmms_output_t *output);
static gboolean xmms_null_format_set (xmms_output_t *output,
                                      const xmms_stream_type_t *format);

/*
 * Plugin header
 */

XMMS_OUTPUT_PLUGIN ("null", "Null Output", XMMS_VERSION,
                    "null output plugin",
                    xmms_null_plugin_setup);

static gboolean
xmms_null_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_null_new;
	methods.destroy = xmms_null_destroy;

	methods.open = xmms_null_open;
	methods.close = xmms_null_close;

	methods.flush = xmms_null_flush;
	methods.format_set = xmms_null_format_set;

	methods.write = xmms_null_write;

	xmms_output_plugin_methods_set (plugin, &methods);

	return TRUE;
}

/*
 * Member functions
 */


/**
 * Creates data for new plugin.
 *
 * @param output The output structure
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_null_new (xmms_output_t *output)
{
	xmms_null_data_t *data;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_null_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_output_private_data_set (output, data);

	xmms_output_stream_type_add (output,
	                             XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

/**
 * Frees data for this plugin allocated in xmms_null_new().
 */
static void
xmms_null_destroy (xmms_output_t *output)
{
	g_return_if_fail (output);

	g_free (xmms_output_private_data_get (output));
}

/**
 * Open audio device.
 *
 * @param output The output structure filled with null data.
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_null_open (xmms_output_t *output)
{
	g_return_val_if_fail (output, FALSE);

	return TRUE;
}

/**
 * CLose audio device.
 *
 * @param output The output structure filled with null data.
 */
static void
xmms_null_close (xmms_output_t *output)
{
	g_return_if_fail (output);
}

/**
 * Set audio format.
 *
 * @param output The output struct containing null data.
 * @param format The new audio format.
 *
 * @return Success/failure
 */
static gboolean
xmms_null_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	xmms_null_data_t *data;
	gint fmt, ch, rate;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (format, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	fmt = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_FORMAT);
	ch = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_CHANNELS);
	rate = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	if (fmt == -1 || ch == -1 || rate == -1) {
		return FALSE;
	}

	/* There will be some quite big rounding errors here.. */
	data->us_per_byte = 1000000/(xmms_sample_size_get (fmt) * ch * rate);

	return TRUE;
}

/**
 * Flush buffer.
 *
 * @param output The output struct containing null data.
 */
static void
xmms_null_flush (xmms_output_t *output)
{
	g_return_if_fail (output);
}

/**
 * Write buffer to the audio device.
 *
 * @param output The output struct containing null data.
 * @param buffer Audio data to be written to audio device.
 * @param len The length of audio data.
 */
static void
xmms_null_write (xmms_output_t *output, gpointer buffer, gint len,
                 xmms_error_t *error)
{
	xmms_null_data_t *data;

	g_return_if_fail (output);
	g_return_if_fail (buffer);

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	g_usleep (len * data->us_per_byte);
}
