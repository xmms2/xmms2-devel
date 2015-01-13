/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

#include <string.h>
#include <ao/ao.h>

/*
 * Type definitions
 */

typedef struct xmms_ao_data_St {
	gint driver_id;
	ao_device *device;
	ao_option *options;
	ao_sample_format format;
} xmms_ao_data_t;

static const xmms_sample_format_t formats[] = {
	XMMS_SAMPLE_FORMAT_S8,
	XMMS_SAMPLE_FORMAT_S16,
	XMMS_SAMPLE_FORMAT_S32,
};

static const int rates[] = {
	8000,
	11025,
	16000,
	22050,
	44100,
	48000,
	96000,
};

/*
 * Function prototypes
 */

static gboolean xmms_ao_plugin_setup (xmms_output_plugin_t *output_plugin);
static gboolean xmms_ao_new (xmms_output_t *output);
static void xmms_ao_destroy (xmms_output_t *output);
static void xmms_ao_flush (xmms_output_t *output);
static gboolean xmms_ao_open (xmms_output_t *output);
static void xmms_ao_close (xmms_output_t *output);
static void xmms_ao_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err);
static gboolean xmms_ao_format_set (xmms_output_t *output, const xmms_stream_type_t *format);
static gboolean xmms_ao_try_format (gint driver_id, ao_option *options, xmms_sample_format_t format,
                                    gint channels, gint samplerate, ao_sample_format *fmt);

/*
 * Plugin header
 */
XMMS_OUTPUT_PLUGIN_DEFINE ("ao",
                           "libao output",
                           XMMS_VERSION,
                           "libao output plugin",
                           xmms_ao_plugin_setup);

static gboolean
xmms_ao_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);
	methods.new = xmms_ao_new;
	methods.destroy = xmms_ao_destroy;
	methods.flush = xmms_ao_flush;

	methods.open = xmms_ao_open;
	methods.close = xmms_ao_close;
	methods.format_set = xmms_ao_format_set;
	methods.write = xmms_ao_write;

	xmms_output_plugin_methods_set (plugin, &methods);

	xmms_output_plugin_config_property_register (plugin,
	                                             "driver",
	                                             "default",
	                                             NULL,
	                                             NULL);

	xmms_output_plugin_config_property_register (plugin,
	                                             "device",
	                                             "default",
	                                             NULL,
	                                             NULL);

	return TRUE;
}

static gboolean
xmms_ao_new (xmms_output_t *output)
{
	xmms_ao_data_t* data;
	xmms_config_property_t *config;
	const gchar *value;
	ao_sample_format format;
	gint i, j, k;

	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_ao_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	ao_initialize ();

	config = xmms_output_config_lookup (output, "driver");
	value = xmms_config_property_get_string (config);

	if (!strcmp (value, "default")) {
		data->driver_id = ao_default_driver_id ();
	} else {
		data->driver_id = ao_driver_id (value);
		if (data->driver_id < 0) {
			xmms_log_error ("Invalid driver id, falling back to default");
			data->driver_id = ao_default_driver_id ();
		}
	}

	if (data->driver_id < 0) {
		/* failed to find a usable audio output device */
		xmms_log_error ("Cannot find usable audio output device!");
		ao_shutdown ();
		return FALSE;
	} else {
		ao_info *info = ao_driver_info (data->driver_id);
		if (info->type != AO_TYPE_LIVE) {
			xmms_log_error ("Selected driver cannot play live output");
			ao_shutdown ();
			return FALSE;
		}
		XMMS_DBG ("Using libao driver %s (%s)", info->name, info->short_name);
	}

	config = xmms_output_config_lookup (output, "device");
	value = xmms_config_property_get_string (config);

	if (!strcmp (value, "default")) {
		data->options = NULL;
	} else {
		ao_device *device;

		data->options = g_malloc (sizeof (ao_option));
		data->options->key = (gchar *) "dev";
		data->options->value = (gchar *) value;
		data->options->next = NULL;

		/* let's just use some common format to check if the device
		 * name is valid */
		format.bits = 16;
		format.rate = 44100;
		format.channels = 2;
		format.byte_format = AO_FMT_NATIVE;

		device = ao_open_live (data->driver_id, &format, data->options);
		if (!device && errno == AO_EOPENDEVICE) {
			xmms_log_error ("Configured device name is incorrect, using default");
			g_free (data->options);
			data->options = NULL;
		} else if (device) {
			if (ao_close (device) == 0) {
				xmms_log_error ("Failed to close libao device");
			}
		}
	}
	data->device = NULL;

	/* probe for supported samplerates */
	for (i=0; i < G_N_ELEMENTS (formats); i++) {
		for (j=0; j < 2; j++) {
			for (k=0; k < G_N_ELEMENTS (rates); k++) {
				if (xmms_ao_try_format (data->driver_id, data->options,
				                        formats[i], j + 1, rates[k],
				                        &format)) {
					data->device = ao_open_live (data->driver_id,
					                             &format,
					                             data->options);
					if (data->device) {
							if (ao_close (data->device) == 0) {
								xmms_log_error ("Failed to close libao device");
							}
							g_memmove (&data->format, &format,
							           sizeof (ao_sample_format));
							xmms_output_format_add (output, formats[i], j + 1,
							                        rates[k]);
					}
				}
			}
		}
	}
	xmms_output_private_data_set (output, data);

	return TRUE;
}

static void
xmms_ao_destroy (xmms_output_t *output)
{
	xmms_ao_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	ao_shutdown ();

	if (data) {
		ao_option *current = data->options;
		while (current) {
			ao_option *next = current->next;
			g_free (current);
			current = next;
		}
	}
	g_free (data);
}

static void
xmms_ao_flush (xmms_output_t *output)
{
	/* not implemented */
}

static gboolean
xmms_ao_open (xmms_output_t *output)
{
	xmms_ao_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	XMMS_DBG ("Opening audio device");

	data->device = ao_open_live (data->driver_id, &data->format, data->options);
	if (!data->device) {
		xmms_log_error ("Cannot open libao output device (errno %d)", errno);
		return FALSE;
	}
	return TRUE;
}

static void
xmms_ao_close (xmms_output_t *output)
{
	xmms_ao_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (ao_close (data->device) == 0) {
		xmms_log_error ("Failed to close libao device");
	}
	data->device = NULL;
}

static void
xmms_ao_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err)
{
	xmms_ao_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (!ao_play (data->device, buffer, len)) {
		ao_close (data->device);
		data->device = NULL;
		xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE,
		                "Error writing to libao, output closed");
	}
}

static gboolean
xmms_ao_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	xmms_ao_data_t *data;
	ao_sample_format oldfmt;
	xmms_sample_format_t sformat;
	gint channels, srate;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	sformat = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_FORMAT);
	channels = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_CHANNELS);
	srate = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_SAMPLERATE);

	XMMS_DBG ("Setting audio format: %d %dch %dHz", sformat, channels, srate);

	g_memmove (&oldfmt, &data->format, sizeof (ao_sample_format));
	if (!xmms_ao_try_format (data->driver_id, data->options, sformat, channels,
	                         srate, &data->format)) {
		xmms_log_error ("Unsupported sample format!");
		return FALSE;
	}

	if (!memcmp (&data->format, &oldfmt, sizeof (ao_sample_format))) {
		/* sample format type same as before, no change */
		return TRUE;
	}

	if (ao_close (data->device) == 0) {
		xmms_log_error ("Failed to close libao device while changing format");
	}

	data->device = ao_open_live (data->driver_id, &data->format, data->options);
	if (!data->device) {
		xmms_log_error ("Weird, cannot reopen libao output device (errno %d)", errno);
		data->device = ao_open_live (data->driver_id, &data->format, data->options);
		return FALSE;
	}

	return TRUE;
}

static gboolean
xmms_ao_try_format (gint driver_id, ao_option *options, xmms_sample_format_t format,
                    gint channels, gint samplerate, ao_sample_format *fmt)
{
	g_return_val_if_fail (fmt, FALSE);

	switch (format) {
		case XMMS_SAMPLE_FORMAT_S8:
			fmt->bits = 8;
			break;
		case XMMS_SAMPLE_FORMAT_S16:
			fmt->bits = 16;
			break;
		case XMMS_SAMPLE_FORMAT_S32:
			fmt->bits = 32;
			break;
		default:
			/* unsupported format */
			return FALSE;
	}
	fmt->channels = channels;
	fmt->rate = samplerate;
	fmt->byte_format = AO_FMT_NATIVE;

	return TRUE;
}
