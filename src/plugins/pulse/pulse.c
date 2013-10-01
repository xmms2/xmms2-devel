/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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

#include <xmms/xmms_outputplugin.h>
#include <xmms/xmms_log.h>


#include <glib.h>

#include "backend.h"

/*
 * Type definitions
 */
typedef struct {
	xmms_pulse *pulse;
} xmms_pulse_data_t;

#define XMMS_PULSE_DEFAULT_NAME "XMMS2"

/*
 * Function prototypes
 */
static gboolean xmms_pulse_plugin_setup (xmms_output_plugin_t *plugin);
static void xmms_pulse_flush (xmms_output_t *output);
static void xmms_pulse_close (xmms_output_t *output);
static void xmms_pulse_write (xmms_output_t *output, gpointer buffer, gint len,
                              xmms_error_t *err);
static gboolean xmms_pulse_open (xmms_output_t *output);
static gboolean xmms_pulse_new (xmms_output_t *output);
static void xmms_pulse_destroy (xmms_output_t *output);
static gboolean xmms_pulse_format_set (xmms_output_t *output,
                                       const xmms_stream_type_t *format);
static gboolean xmms_pulse_volume_set (xmms_output_t *output,
                                       const gchar *channel,
                                       guint volume);
static gboolean xmms_pulse_volume_get (xmms_output_t *output,
                                       const gchar **names,
                                       guint *values,
                                       guint *num_channels);


/*
 * Plugin header
 */
XMMS_OUTPUT_PLUGIN_DEFINE ("pulse", "PulseAudio Output", XMMS_VERSION,
                    "Output to a PulseAudio server",
                    xmms_pulse_plugin_setup);

static gboolean
xmms_pulse_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_pulse_new;
	methods.destroy = xmms_pulse_destroy;
	methods.open = xmms_pulse_open;
	methods.close = xmms_pulse_close;
	methods.write = xmms_pulse_write;
	methods.flush = xmms_pulse_flush;
	methods.format_set = xmms_pulse_format_set;
	methods.volume_set = xmms_pulse_volume_set;
	methods.volume_get = xmms_pulse_volume_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	xmms_output_plugin_config_property_register (plugin, "server", "",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "sink", "",
	                                             NULL, NULL);
	xmms_output_plugin_config_property_register (plugin, "name", "XMMS2",
	                                             NULL, NULL);

	return TRUE;
}


static gboolean
xmms_pulse_new (xmms_output_t *output)
{
	xmms_pulse_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_pulse_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_output_private_data_set (output, data);

	xmms_output_stream_type_add (output,
	                             XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT, XMMS_SAMPLE_FORMAT_U8,
	                             XMMS_STREAM_TYPE_END);
	xmms_output_stream_type_add (output,
	                             XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT, XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_END);
	xmms_output_stream_type_add (output,
	                             XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT, XMMS_SAMPLE_FORMAT_S32,
	                             XMMS_STREAM_TYPE_END);
	xmms_output_stream_type_add (output,
	                             XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT, XMMS_SAMPLE_FORMAT_FLOAT,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}


static void
xmms_pulse_destroy (xmms_output_t *output)
{
	xmms_pulse_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	g_free (data);
}


static gboolean
xmms_pulse_open (xmms_output_t *output)
{
	xmms_pulse_data_t *data;
	const xmms_config_property_t *val;
	const gchar *server, *name;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	val = xmms_output_config_lookup (output, "server");
	server = xmms_config_property_get_string (val);
	if (server && *server == '\0')
		server = NULL;

	val = xmms_output_config_lookup (output, "name");
	name = xmms_config_property_get_string (val);
	if (!name || *name == '\0')
		name = XMMS_PULSE_DEFAULT_NAME;

	data->pulse = xmms_pulse_backend_new (server, name, NULL);
	if (!data->pulse)
		return FALSE;

	return TRUE;
}


static void
xmms_pulse_close (xmms_output_t *output)
{
	xmms_pulse_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->pulse) {
		xmms_pulse_backend_free (data->pulse);
		data->pulse = NULL;
	}
}


static gboolean
xmms_pulse_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	xmms_pulse_data_t *data;
	const xmms_config_property_t *val;
	const gchar *sink, *name;
	xmms_sample_format_t xmms_format;
	gint channels;
	gint samplerate;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	xmms_format = xmms_stream_type_get_int (format,
	                                        XMMS_STREAM_TYPE_FMT_FORMAT);
	channels = xmms_stream_type_get_int (format,
	                                     XMMS_STREAM_TYPE_FMT_CHANNELS);
	samplerate = xmms_stream_type_get_int (format,
	                                       XMMS_STREAM_TYPE_FMT_SAMPLERATE);

	val = xmms_output_config_lookup (output, "sink");
	sink = xmms_config_property_get_string (val);
	if (sink && *sink == '\0')
		sink = NULL;

	val = xmms_output_config_lookup (output, "name");
	name = xmms_config_property_get_string (val);
	if (!name || *name == '\0')
		name = XMMS_PULSE_DEFAULT_NAME;

	if (!xmms_pulse_backend_set_stream (data->pulse, name, sink, xmms_format,
	                                    samplerate, channels, NULL))
		return FALSE;

	return TRUE;
}


static void
xmms_pulse_flush (xmms_output_t *output)
{
	xmms_pulse_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->pulse)
		xmms_pulse_backend_flush (data->pulse, NULL);
}


static gboolean
xmms_pulse_volume_set (xmms_output_t *output,
                      const gchar *channel_name, guint volume)
{
	xmms_pulse_data_t *data;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (channel_name, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	g_return_val_if_fail (volume <= 100, FALSE);

	return xmms_pulse_backend_volume_set (data->pulse, volume);
}


static gboolean
xmms_pulse_volume_get (xmms_output_t *output, const gchar **names,
                       guint *values, guint *num_channels)
{
	xmms_pulse_data_t *data;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	g_return_val_if_fail (num_channels, FALSE);

	if (!*num_channels) {
		*num_channels = 1;
		return TRUE;
	}

	g_return_val_if_fail (*num_channels == 1, FALSE);
	g_return_val_if_fail (names, FALSE);
	g_return_val_if_fail (values, FALSE);

	names[0] = "master";

	return xmms_pulse_backend_volume_get (data->pulse, &values[0]);
}


static void
xmms_pulse_write (xmms_output_t *output, gpointer buffer, gint len,
                  xmms_error_t *err)
{
	xmms_pulse_data_t *data;

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	xmms_pulse_backend_write (data->pulse, buffer, len, NULL);
}
