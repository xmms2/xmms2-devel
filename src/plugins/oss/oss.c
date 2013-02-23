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


#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"
#include "xmms_configuration.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#include <glib.h>

/* These are new defines of OSS4 that can't be found from old
 * version of soundcard.h, therefore define them manually to avoid
 * OSS4 compile time dependency that can cause some trouble... */
#ifndef SNDCTL_DSP_GETPLAYVOL
#define SNDCTL_DSP_GETPLAYVOL       _IOR ('P',  24, int)
#endif

#ifndef SNDCTL_DSP_SETPLAYVOL
#define SNDCTL_DSP_SETPLAYVOL       _IOWR('P',  24, int)
#endif

#ifndef OSS_GETVERSION
#define OSS_GETVERSION              _IOR ('M', 118, int)
#endif

/*
 * Type definitions
 */

typedef struct xmms_oss_data_St {
	gint fd;
	gint mixer_fd;
	gboolean have_mixer;
	gboolean oss4_mixer;
} xmms_oss_data_t;

static const struct {
	xmms_sample_format_t xmms_fmt;
	int oss_fmt;
} formats[] = {
	{XMMS_SAMPLE_FORMAT_U8, AFMT_U8},
	{XMMS_SAMPLE_FORMAT_S8, AFMT_S8},
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	{XMMS_SAMPLE_FORMAT_S16, AFMT_S16_LE},
	{XMMS_SAMPLE_FORMAT_U16, AFMT_U16_LE},
#else
	{XMMS_SAMPLE_FORMAT_S16, AFMT_S16_BE},
	{XMMS_SAMPLE_FORMAT_U16, AFMT_U16_BE},
#endif
};

static const int rates[] = {
	1337,
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

static gboolean xmms_oss_plugin_setup (xmms_output_plugin_t *output_plugin);
static gboolean xmms_oss_open (xmms_output_t *output);
static gboolean xmms_oss_new (xmms_output_t *output);
static void xmms_oss_destroy (xmms_output_t *output);
static void xmms_oss_close (xmms_output_t *output);
static void xmms_oss_flush (xmms_output_t *output);
static void xmms_oss_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err);
static guint xmms_oss_buffersize_get (xmms_output_t *output);
static gboolean xmms_oss_format_set (xmms_output_t *output, const xmms_stream_type_t *format);
static gboolean xmms_oss_volume_set (xmms_output_t *output, const gchar *channel, guint volume);
static gboolean xmms_oss_volume_get (xmms_output_t *output,
                                     gchar const **names, guint *values,
                                     guint *num_channels);

/*
 * Plugin header
 */
XMMS_OUTPUT_PLUGIN ("oss",
                    "OSS Output",
                    XMMS_VERSION,
                    "OpenSoundSystem output plugin",
                    xmms_oss_plugin_setup);

static gboolean
xmms_oss_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);
	methods.new = xmms_oss_new;
	methods.destroy = xmms_oss_destroy;

	methods.open = xmms_oss_open;
	methods.close = xmms_oss_close;

	methods.flush = xmms_oss_flush;
	methods.format_set = xmms_oss_format_set;

	methods.volume_get = xmms_oss_volume_get;
	methods.volume_set = xmms_oss_volume_set;

	methods.write = xmms_oss_write;

	methods.latency_get = xmms_oss_buffersize_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	/*
	  xmms_plugin_info_add (plugin, "URL", "http://xmms2.org/");
	  xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	*/

	xmms_output_plugin_config_property_register (plugin,
	                                             "mixer",
	                                             "/dev/mixer",
	                                             NULL,
	                                             NULL);

	xmms_output_plugin_config_property_register (plugin,
	                                             "device",
	                                             OSS_DEFAULT_DEVICE,
	                                             NULL,
	                                             NULL);

	return TRUE;
}

/*
 * Member functions
 */
static gboolean
xmms_oss_volume_set (xmms_output_t *output,
                     const gchar *channel, guint volume)
{
	xmms_oss_data_t *data;
	gint left, right, tmp = 0;
	gint ret;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (channel, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!data->have_mixer)
		return FALSE;

	/* get current volume */
	if (data->oss4_mixer) {
		ret = ioctl (data->fd, SNDCTL_DSP_GETPLAYVOL, &tmp);
	} else {
		ret = ioctl (data->mixer_fd, SOUND_MIXER_READ_PCM, &tmp);
	}
	if (ret == -1) {
		XMMS_DBG ("Mixer ioctl failed: %s", strerror (errno));
		return FALSE;
	}

	right = (tmp & 0xFF00) >> 8;
	left = (tmp & 0x00FF);

	/* update the channel volumes */
	if (!strcmp (channel, "right")) {
		right = volume;
	} else if (!strcmp (channel, "left")) {
		left = volume;
	} else
		return FALSE;

	/* and write it back again */
	tmp = (right << 8) | left;

	if (data->oss4_mixer) {
		ret = ioctl (data->fd, SNDCTL_DSP_SETPLAYVOL, &tmp);
	} else {
		ret = ioctl (data->mixer_fd, SOUND_MIXER_WRITE_PCM, &tmp);
	}
	if (ret == -1) {
		XMMS_DBG ("Mixer ioctl failed: %s", strerror (errno));
		return FALSE;
	}

	return TRUE;
}

/** @todo support PCM/MASTER?*/
static gboolean
xmms_oss_volume_get (xmms_output_t *output, gchar const **names, guint *values,
                     guint *num_channels)
{
	xmms_oss_data_t *data;
	gint tmp = 0, i, ret;
	struct {
		const gchar *name;
		gint value;
	} channel_map[] = {
		{"right", 0},
		{"left", 0}
	};

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!data->have_mixer)
		return FALSE;

	if (!*num_channels) {
		*num_channels = 2;
		return TRUE;
	}

	if (data->oss4_mixer) {
		ret = ioctl (data->fd, SNDCTL_DSP_GETPLAYVOL, &tmp);
	} else {
		ret = ioctl (data->mixer_fd, SOUND_MIXER_READ_PCM, &tmp);
	}
	if (ret == -1) {
		XMMS_DBG ("Mixer ioctl failed: %s", strerror (errno));

		/* Disable mixer support, because volume getting failed */
		xmms_log_error ("Disabling mixer support!");
		data->have_mixer = FALSE;

		return FALSE;
	}

	channel_map[0].value = (tmp & 0xFF00) >> 8;
	channel_map[1].value = (tmp & 0x00FF);

	for (i = 0; i < 2; i++) {
		names[i] = channel_map[i].name;
		values[i] = channel_map[i].value;
	}

	return TRUE;

}

static guint
xmms_oss_buffersize_get (xmms_output_t *output)
{
	int err = 0;
	guint ret = 0;
	xmms_oss_data_t *data;
	audio_buf_info buf_info;

	g_return_val_if_fail (output, 0);

	data = xmms_output_private_data_get (output);

	err = ioctl (data->fd, SNDCTL_DSP_GETOSPACE, &buf_info);
	if (!err) {
		ret = (buf_info.fragstotal * buf_info.fragsize) - buf_info.bytes;
	}

	return ret;
}

static void
xmms_oss_flush (xmms_output_t *output)
{
	xmms_oss_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	/* reset soundcard buffer */
	ioctl (data->fd, SNDCTL_DSP_RESET, 0);

}

static gboolean
xmms_oss_open (xmms_output_t *output)
{
	xmms_oss_data_t *data;
	const xmms_config_property_t *val;
	const gchar *dev;
	guint param;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);

	XMMS_DBG ("xmms_oss_open (%p)", output);

	val = xmms_output_config_lookup (output, "device");
	dev = xmms_config_property_get_string (val);

	data->fd = open (dev, O_RDWR);
	if (data->fd == -1)
		return FALSE;

	param = (32 << 16) | 0xC; /* 32 * 4096 */
	if (ioctl (data->fd, SNDCTL_DSP_SETFRAGMENT, &param) == -1)
		goto error;

	if (data->oss4_mixer) {
		/* OSS4 mixer is using the device file descriptor */
		data->have_mixer = TRUE;
	}

	return TRUE;

error:
	close (data->fd);
	if (data->mixer_fd != -1)
		close (data->mixer_fd);
	g_free (data);
	return FALSE;

}

static gboolean
xmms_oss_new (xmms_output_t *output)
{
	xmms_oss_data_t *data;
	xmms_config_property_t *val;
	const gchar *dev;
	const gchar *mixdev;
	int i,j,k, param, fmts, fd, version;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_oss_data_t, 1);
	xmms_output_private_data_set (output, data);

	val = xmms_output_config_lookup (output, "device");
	dev = xmms_config_property_get_string (val);

	XMMS_DBG ("device = %s", dev);

	fd = open (dev, O_WRONLY);
	if (fd == -1)
		return FALSE;

	if (ioctl (fd, OSS_GETVERSION, &version) != -1) {
		XMMS_DBG ("Found OSS version 0x%06x", version);
		if (version >= 0x040000) {
			data->oss4_mixer = TRUE;
		}
	}

	if (ioctl (fd, SNDCTL_DSP_GETFMTS, &fmts) == -1)
		goto err;

	for (i = 0; i < G_N_ELEMENTS (formats); i++) {
		if (formats[i].oss_fmt & fmts) {
			for (j = 0; j < 2; j++) {
				gboolean added = FALSE;
				param = formats[i].oss_fmt;
				if (ioctl (fd, SNDCTL_DSP_SETFMT, &param) == -1)
					continue;
				param = j;
				if (ioctl (fd, SNDCTL_DSP_STEREO, &param) == -1 || param != j)
					continue;

				for (k = 0; k < G_N_ELEMENTS (rates); k++) {
					param = rates[k];
					if (ioctl (fd, SNDCTL_DSP_SPEED, &param) == -1 || param != rates[k])
						continue;


					xmms_output_format_add (output, formats[i].xmms_fmt, j + 1, rates[k]);
					added = TRUE;
				}
				if (!added) {
					XMMS_DBG ("Adding fallback format...");
					xmms_output_format_add (output, formats[i].xmms_fmt, j + 1, param);
				}
			}
		}
	}

	close (fd);

	val = xmms_output_config_lookup (output, "mixer");
	mixdev = xmms_config_property_get_string (val);

	if (data->oss4_mixer) {
		/* OSS4 doesn't use the mixer device, mark it as not opened */
		data->mixer_fd = -1;
	} else {
		/* Open mixer here. I am not sure this is entirely correct. */
		data->mixer_fd = open (mixdev, O_RDWR);
		if (data->mixer_fd == -1)
			data->have_mixer = FALSE;
		else
			data->have_mixer = TRUE;
	}

	XMMS_DBG ("OpenSoundSystem initilized!");

	XMMS_DBG ("Have mixer = %d", (data->have_mixer || data->oss4_mixer));

	return TRUE;

 err:
	close (fd);
	return FALSE;
}

static void
xmms_oss_destroy (xmms_output_t *output)
{
	xmms_oss_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->mixer_fd != -1) {
		close (data->mixer_fd);
	}

	g_free (data);
}

static gboolean
xmms_oss_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	guint param;
	int i, fmt;
	xmms_oss_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	/* we must first drain the buffer.. */
	ioctl (data->fd, SNDCTL_DSP_SYNC, 0);
        ioctl (data->fd, SNDCTL_DSP_RESET, 0);

	fmt = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_FORMAT);
	param = -1;
	for (i = 0; i < G_N_ELEMENTS (formats); i++) {
		if (formats[i].xmms_fmt == fmt) {
			param = formats[i].oss_fmt;
			break;
		}
	}
	g_return_val_if_fail (param != -1, FALSE);

	if (ioctl (data->fd, SNDCTL_DSP_SETFMT, &param) == -1)
		goto error;

	param = (xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_CHANNELS) == 2);
	if (ioctl (data->fd, SNDCTL_DSP_STEREO, &param) == -1)
		goto error;

	param = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	if (ioctl (data->fd, SNDCTL_DSP_SPEED, &param) == -1)
		goto error;

	return TRUE;
 error:
	return FALSE;
}

static void
xmms_oss_close (xmms_output_t *output)
{
	xmms_oss_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->oss4_mixer) {
		/* Mark that the file descriptor is closed and we have no mixer */
		data->have_mixer = FALSE;
	}
	close (data->fd);
}

static void
xmms_oss_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err)
{
	xmms_oss_data_t *data;

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	data = xmms_output_private_data_get (output);

	write (data->fd, buffer, len);
}
