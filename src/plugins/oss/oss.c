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




#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/config.h"
#include "xmms/signal_xmms.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <stdio.h>


#include <glib.h>

/*
 * Type definitions
 */

typedef struct xmms_oss_data_St {
	gint fd;
	gint mixer_fd;
	gboolean have_mixer;

	xmms_config_value_t *mixer;

	/* ugly lock to avoid problems in kernel api */
	GMutex *mutex;
} xmms_oss_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_oss_open (xmms_output_t *output);
static gboolean xmms_oss_new (xmms_output_t *output);
static void xmms_oss_close (xmms_output_t *output);
static void xmms_oss_flush (xmms_output_t *output);
static void xmms_oss_write (xmms_output_t *output, gchar *buffer, gint len);
static guint xmms_oss_samplerate_set (xmms_output_t *output, guint rate);
static guint xmms_oss_buffersize_get (xmms_output_t *output);
static gboolean xmms_oss_mixer_set (xmms_output_t *output, gint left, gint right);
static gboolean xmms_oss_mixer_get (xmms_output_t *output, gint *left, gint *right);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "oss",
			"OSS Output " XMMS_VERSION,
			"OpenSoundSystem output plugin");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, xmms_oss_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, xmms_oss_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_oss_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_oss_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, xmms_oss_samplerate_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET, xmms_oss_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, xmms_oss_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_GET, xmms_oss_mixer_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_SET, xmms_oss_mixer_set);

	xmms_plugin_config_value_register(plugin, 
		  	   	 	   "mixer",
					   "/dev/mixer",
					   NULL,
					   NULL);
	
	xmms_plugin_config_value_register (plugin,
					   "device",
					   "/dev/dsp",
					   NULL,
					   NULL);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_oss_mixer_set (xmms_output_t *output, gint left, gint right)
{
	xmms_oss_data_t *data;
	gint volume;

	g_return_val_if_fail (output, FALSE);
	
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!data->have_mixer)
		return FALSE;

	XMMS_DBG ("Setting mixer to %d/%d", left, right);

	volume = (right << 8) | left;

	ioctl (data->mixer_fd, SOUND_MIXER_WRITE_PCM, &volume);

	return TRUE;
}

/** @todo support PCM/MASTER?*/
static gboolean
xmms_oss_mixer_get (xmms_output_t *output, gint *left, gint *right)
{
	xmms_oss_data_t *data;
	gint volume;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!data->have_mixer)
		return FALSE;

	ioctl (data->mixer_fd, SOUND_MIXER_READ_PCM, &volume);

	*right = (volume & 0xFF00) >> 8;
	*left = (volume & 0x00FF);

	return TRUE;
}

static guint
xmms_oss_buffersize_get (xmms_output_t *output)
{
	xmms_oss_data_t *data;
	audio_buf_info buf_info;

	g_return_val_if_fail (output, 0);

	data = xmms_output_private_data_get (output);

	if(!ioctl(data->fd, SNDCTL_DSP_GETOSPACE, &buf_info)){
		return (buf_info.fragstotal * buf_info.fragsize) - buf_info.bytes;
	}
	return 0;
}

static void
xmms_oss_flush (xmms_output_t *output)
{
	xmms_oss_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	/* reset soundcard buffer */
	g_mutex_lock (data->mutex);
	ioctl (data->fd, SNDCTL_DSP_RESET, 0);
	g_mutex_unlock (data->mutex);

}

static gboolean
xmms_oss_open (xmms_output_t *output)
{
	xmms_oss_data_t *data;
	const xmms_config_value_t *val;
	const gchar *dev;
	guint param;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	
	XMMS_DBG ("xmms_oss_open (%p)", output);

	val = xmms_plugin_config_lookup (xmms_output_plugin_get (output), "device");
	dev = xmms_config_value_string_get (val);

	XMMS_DBG ("device = %s", dev);

	data->fd = open (dev, O_WRONLY);
	if (data->fd == -1)
		return FALSE;

	
	param = (32 << 16) | 0xC; /* 32 * 4096 */
	if (ioctl (data->fd, SNDCTL_DSP_SETFRAGMENT, &param) == -1)
		goto error;
	param = AFMT_S16_NE;
	if (ioctl (data->fd, SNDCTL_DSP_SETFMT, &param) == -1)
		goto error;
	param = 1;
	if (ioctl (data->fd, SNDCTL_DSP_STEREO, &param) == -1)
		goto error;
	param = 44100;
	if (ioctl (data->fd, SNDCTL_DSP_SPEED, &param) == -1)
		goto error;

	return TRUE;

error:
	close (data->fd);
	if (data->have_mixer)
		close (data->mixer_fd);
	g_free (data);
	return FALSE;

}

static gboolean
xmms_oss_new (xmms_output_t *output)
{
	xmms_oss_data_t *data;
	xmms_config_value_t *val;
	const gchar *mixdev;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_oss_data_t, 1);
	data->mutex = g_mutex_new ();

	val = xmms_plugin_config_lookup (
			xmms_output_plugin_get (output), "mixer");
	mixdev = xmms_config_value_string_get (val);

	/* Open mixer here. I am not sure this is entirely correct. */
	data->mixer_fd = open (mixdev, O_RDONLY);
	if (!data->mixer_fd == -1)
		data->have_mixer = FALSE;
	else
		data->have_mixer = TRUE;

	XMMS_DBG ("Have mixer = %d", data->have_mixer);

	/* retrive keys for mixer settings. but ignore current values */
	data->mixer = xmms_plugin_config_lookup (
			xmms_output_plugin_get (output), "volume");

	/* since we don't have this data when we are register the configvalue
	   we need to set the callback here */
	xmms_config_value_callback_set (data->mixer, 
					NULL,
					(gpointer) output);

	xmms_output_private_data_set (output, data);

	XMMS_DBG ("OpenSoundSystem initilized!");
	
	return TRUE;
}

static guint
xmms_oss_samplerate_set (xmms_output_t *output, guint rate)
{
	xmms_oss_data_t *data;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	/* we must first drain the buffer.. */
	ioctl (data->fd, SNDCTL_DSP_SYNC, 0);

	if (ioctl (data->fd, SNDCTL_DSP_SPEED, &rate) == -1) {
		xmms_log_error ("Error setting samplerate");
		return 0;
	}
	return rate;
}

static void
xmms_oss_close (xmms_output_t *output)
{
	xmms_oss_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	close (data->fd);
}

static void
xmms_oss_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_oss_data_t *data;
	
	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	data = xmms_output_private_data_get (output);

	/* make sure that we don't flush buffers when we are writing */
	g_mutex_lock (data->mutex);
	write (data->fd, buffer, len);
	g_mutex_unlock (data->mutex);

}
