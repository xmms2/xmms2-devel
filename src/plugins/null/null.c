/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include <glib.h>
#include <time.h>
#include <errno.h>

/*
 * Type definitions
 */
typedef struct xmms_null_data_St {
	xmms_audio_format_t *format;
} xmms_null_data_t;

/*
 * Function prototypes
 */
static void xmms_null_flush (xmms_output_t *output);
static void xmms_null_write (xmms_output_t *output, gchar *buffer, gint len);
static gboolean xmms_null_open (xmms_output_t *output);
static void xmms_null_close (xmms_output_t *output);
static gboolean xmms_null_new (xmms_output_t *output);
static void xmms_null_destroy (xmms_output_t *output);
static gboolean xmms_null_format_set (xmms_output_t *output, xmms_audio_format_t *format);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT,
				  XMMS_OUTPUT_PLUGIN_API_VERSION,
				  "null",
				  "Null Output" XMMS_VERSION,
				  "Null output plugin");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE,
	                        xmms_null_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN,
	                        xmms_null_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE,
	                        xmms_null_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW,
	                        xmms_null_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY,
	                        xmms_null_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH,
	                        xmms_null_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FORMAT_SET,
	                        xmms_null_format_set);

	return plugin;
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

	xmms_sample_format_t formats[] = {
		XMMS_SAMPLE_FORMAT_U8,
		XMMS_SAMPLE_FORMAT_S8,
		XMMS_SAMPLE_FORMAT_S16,
		XMMS_SAMPLE_FORMAT_U16,
		XMMS_SAMPLE_FORMAT_S32,
		XMMS_SAMPLE_FORMAT_U32,
		XMMS_SAMPLE_FORMAT_FLOAT,
		XMMS_SAMPLE_FORMAT_DOUBLE
	};
	int rates[] = {
		8000,
		11025,
		16000,
		22050,
		44100,
		48000,
		96000
	};
	gint i, j;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_null_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_output_private_data_set (output, data);

	for (i = 0; i < G_N_ELEMENTS (formats); i++) {
		for (j = 0; j < G_N_ELEMENTS (rates); j++) {
			xmms_output_format_add (output, formats[i], 1, rates[j]);
			xmms_output_format_add (output, formats[i], 2, rates[j]);
		}
	}

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
xmms_null_format_set (xmms_output_t *output, xmms_audio_format_t *format)
{
	xmms_null_data_t *data;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (format, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	data->format = format;

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
xmms_null_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_null_data_t *data;
	guint ms;
	struct timespec req, rem;

	g_return_if_fail (output);
	g_return_if_fail (buffer);

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	ms = xmms_sample_bytes_to_ms (data->format, len);

	req.tv_sec = ms / 1000;
	req.tv_nsec = (ms % 1000) * 1000 * 1000;

	while (nanosleep (&req, &rem) == -1 && errno == EINTR) {
		req = rem;
	}
}
