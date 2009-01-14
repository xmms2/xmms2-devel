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


#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <errno.h>


/*
 * Type definitions
 */

typedef struct xmms_sun_data_St {
	gint fd;
	gint mixer_fd;
	gboolean have_mixer;
	guint samplerate;
	guint channels;
	guint precision;
	guint encoding;
	xmms_config_property_t *mixer_conf;
	gchar *mixer_voldev;
} xmms_sun_data_t;

static const struct {
	xmms_sample_format_t xmms_fmt;
	guint sun_encoding;
	guint sun_precision;
} formats[] = {
	{XMMS_SAMPLE_FORMAT_U8, AUDIO_ENCODING_ULAW, 8},
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	{XMMS_SAMPLE_FORMAT_S16, AUDIO_ENCODING_SLINEAR_LE, 16},
	{XMMS_SAMPLE_FORMAT_U16, AUDIO_ENCODING_ULINEAR_LE, 16},
#else
	{XMMS_SAMPLE_FORMAT_S16, AUDIO_ENCODING_SLINEAR_BE, 16},
	{XMMS_SAMPLE_FORMAT_U16, AUDIO_ENCODING_ULINEAR_BE, 16},
#endif
};


/* rates */
static const int rates[] = {
	8000,
	11025,
	16000,
	22050,
	44100,
	48000,
	96000
};


/*
 * Function prototypes
 */

static gboolean xmms_sun_plugin_setup (xmms_output_plugin_t *plugin);
static gboolean xmms_sun_open (xmms_output_t *output);
static gboolean xmms_sun_new (xmms_output_t *output);
static void xmms_sun_destroy (xmms_output_t *output);
static void xmms_sun_close (xmms_output_t *output);
static void xmms_sun_flush (xmms_output_t *output);
static void xmms_sun_write (xmms_output_t *output, gpointer buffer,
                            gint len, xmms_error_t *error);
static guint xmms_sun_buffersize_get (xmms_output_t *output);
static gboolean xmms_sun_format_set (xmms_output_t *output,
                                     const xmms_stream_type_t *format);


/*
 * Plugin header
 */

XMMS_OUTPUT_PLUGIN ("sun", "Sun Output", XMMS_VERSION,
                    "OpenBSD output plugin", xmms_sun_plugin_setup);

static gboolean
xmms_sun_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_sun_new;
	methods.destroy = xmms_sun_destroy;
	methods.open = xmms_sun_open;
	methods.close = xmms_sun_close;
	methods.flush = xmms_sun_flush;
	methods.format_set = xmms_sun_format_set;
	methods.write = xmms_sun_write;
	methods.latency_get = xmms_sun_buffersize_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	xmms_output_plugin_config_property_register (plugin, "device",
	                                             "/dev/audio",
	                                             NULL, NULL);

	xmms_output_plugin_config_property_register (plugin, "mixer",
	                                             "/dev/mixer",
	                                             NULL, NULL);

	return TRUE;
}


/*
 * Member functions
 */


/**
 * Get buffersize
 *
 * @return number of bending bytes
 */
static guint
xmms_sun_buffersize_get (xmms_output_t *output)
{
	xmms_sun_data_t *data;
	audio_info_t info;
	guint pending_bytes;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	AUDIO_INITINFO (&info);

	if (ioctl (data->fd, AUDIO_GETINFO, &info) < 0) {
		return FALSE;
	}

	pending_bytes = info.play.seek * (info.play.precision / 8);

	return pending_bytes;
}


/**
 * Flush buffer.
 *
 * @param output The output struct containing null data.
 */
static void
xmms_sun_flush (xmms_output_t *output)
{
	xmms_sun_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	ioctl (data->fd, AUDIO_FLUSH, 0);
}


/**
 * Open audio device.
 *
 * @param output The output structure filled with null data.
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_sun_open (xmms_output_t *output)
{
	xmms_sun_data_t *data;
	audio_info_t info;
	audio_encoding_t enc;
	const xmms_config_property_t *val;
	const gchar *dev;

	XMMS_DBG ("XMMS_SUN_OPEN");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);

	XMMS_DBG ("xmms_sun_open (%p)", output);

	g_return_val_if_fail (data, FALSE);

	val = xmms_output_config_lookup (output, "device");
	dev = xmms_config_property_get_string (val);

	XMMS_DBG ("Opening device: %s", dev);

	AUDIO_INITINFO (&info);

	data->fd = open (dev, O_WRONLY);
	if (data->fd < 0) {
		xmms_log_error ("%s: %s", dev, (gchar *)strerror (errno));
		return FALSE;
	}

	enc.index = 0;
	while (ioctl (data->fd, AUDIO_GETENC, &enc) == 0 &&
	       enc.encoding != AUDIO_ENCODING_SLINEAR_LE) {
		enc.index++;
	}

	info.play.encoding = enc.encoding;
	info.play.precision = enc.precision;
	info.play.channels = 2;
	info.play.sample_rate = 44100;
	info.mode = AUMODE_PLAY;

	if (ioctl (data->fd, AUDIO_SETINFO, &info) < 0) {
		return FALSE;
	}
	return TRUE;
}


/**
 * Creates data for new plugin.
 *
 * @param output The output structure
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_sun_new (xmms_output_t *output)
{
	xmms_sun_data_t *data;
	audio_info_t info;
	audio_encoding_t enc;
	const xmms_config_property_t *val;
	const gchar *mixdev;
	const gchar *dev;
	gint tmp_audio_fd;
	gint i, j, k;
	gint support = 0;

	g_return_val_if_fail (output, FALSE);

	val = xmms_output_config_lookup (output, "device");
	dev = xmms_config_property_get_string (val);

	XMMS_DBG ("device = %s", dev);

	tmp_audio_fd = open (dev, O_WRONLY);
	if (tmp_audio_fd < 0) {
		return FALSE;
	}

	data = g_new0 (xmms_sun_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	val = xmms_output_config_lookup (output,  "mixer");
	mixdev = xmms_config_property_get_string (val);

	data->mixer_fd = open (mixdev, O_WRONLY);

	if (data->mixer_fd != -1) {
		data->have_mixer = FALSE;
	} else {
		data->have_mixer = TRUE;
	}

	XMMS_DBG ("Have mixer = %d", data->have_mixer);

	data->mixer_conf = xmms_output_config_lookup (output, "volume");
	xmms_output_private_data_set (output, data);

	AUDIO_INITINFO (&info);

	for (i = 0; i < G_N_ELEMENTS (formats); i++) {
		support = 0;
		enc.index = 0;

		while (ioctl (tmp_audio_fd, AUDIO_GETENC, &enc) == 0) {
			if (enc.encoding == formats[i].sun_encoding &&
			    enc.precision == formats[i].sun_precision) {
				support = 1;
				break;
			}
			enc.index++;
		}

		if (support) {
			info.play.encoding = enc.encoding;
			info.play.precision = enc.precision;

			for (j = 0; j < G_N_ELEMENTS (rates); j++) {
				for (k = 1; k <= 2; k++) {
				info.play.sample_rate = rates[j];
					info.play.channels = k;
					if (ioctl (tmp_audio_fd, AUDIO_SETINFO, &info) == 0) {
						xmms_output_format_add (output, formats[i].xmms_fmt,
						                        info.play.channels,
						                        info.play.sample_rate);
					}
				}
			}
		}
	}

	close (tmp_audio_fd);

	XMMS_DBG ("Sun Driver initiliazed!");

	return TRUE;
}


/**
 * Frees data for this plugin.
 */
static void
xmms_sun_destroy (xmms_output_t *output)
{
	xmms_sun_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->have_mixer) {
		close (data->mixer_fd);
	}

	close (data->fd);
	g_free (data);
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
xmms_sun_format_set (xmms_output_t *output,
                     const xmms_stream_type_t *format)
{
	xmms_sun_data_t *data;
	audio_info_t info;
	gint i;
	guint fmt;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (format, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	AUDIO_INITINFO (&info);

	fmt = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_FORMAT);
	for (i = 0; i < G_N_ELEMENTS (formats); i++) {
		if (formats[i].xmms_fmt == fmt) {
			info.play.encoding = formats[i].sun_encoding;
			info.play.precision = formats[i].sun_precision;
			break;
		}
	}

	if (ioctl (data->fd, AUDIO_SETINFO, &info) < 0 ) {
		return FALSE;
	} else {
		data->samplerate = info.play.sample_rate;
		data->channels = info.play.channels;
		data->precision = info.play.precision;
		data->encoding = info.play.encoding;

		xmms_output_private_data_set (output, data);
	}

	return TRUE;
}


/**
 * Close audio device.
 *
 * @param output The output structure filled with sun data.
 */
static void
xmms_sun_close (xmms_output_t *output)
{
	xmms_sun_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (close (data->fd) < 0) {
		xmms_log_error ("Unable to close device (%s)", strerror (errno));
	}
}


/**
 * Write buffer to the audio device.
 * @param output The output struct containing sun data.
 * @param buffer Audio data to be written to the audio device.
 * @param len The length of audio data.
 */
static void
xmms_sun_write (xmms_output_t *output, gpointer buffer,
                gint len, xmms_error_t *err)
{
	xmms_sun_data_t *data;
	gint written;

	g_return_if_fail (len);
	g_return_if_fail (buffer);

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	while (len > 0) {
		written = write (data->fd, buffer, len);
		if (written == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				xmms_log_fatal ("Something is wrong: (%s)",
				                strerror (written));
			}
		}
		buffer += written;
		len -= written;
	}
}
