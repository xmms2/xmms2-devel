/** @file sun.c 
 *  Output plugin for the OpenBSD SUN architecture. 
 *
 *  Copyright (C) 2003-2006 XMMS2 Team
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
 *
 *  @todo Proper error handling and less buskis code for mixer.
 *  @todo Handle config stuff nice.
 */

#include "xmms/xmms_defs.h"
#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>

#include <sys/ioctl.h>
#include <sys/audioio.h>

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>


/*
 *  Defines - not used at all, remove?
 */
#define SUN_DEFAULT_BLOCKSIZE           8800
#define SUN_DEFAULT_BUFFER_SIZE         8800
#define SUN_MIN_BUFFER_SIZE             14336
#define SUN_DEFAULT_PREBUFFER_SIZE      25
#define SUN_VERSION                     "0.6"

/*
 * Type definitions
 */

typedef struct xmms_sun_data_St {
	gint fd;
	gint mixerfd;

	gboolean have_mixer;

	gint req_pre_buffer_size;
	gint req_buffer_size;

	guint samplerate;
	guint channels;
	guint precision;
	guint encoding;

	xmms_config_property_t *mixer_conf;
	gchar *mixer_voldev;
} xmms_sun_data_t;

static struct {
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


#ifdef XMMS_OS_SOLARIS
guint enqued_bytes = 0;
#endif

/* 
 * possible rates 
 */
static int rates[] = {
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
static void xmms_sun_flush (xmms_output_t *output);
static void xmms_sun_close (xmms_output_t *output);
static void xmms_sun_write (xmms_output_t *output, gchar *buffer, gint len);
static void xmms_sun_mixer_config_changed (xmms_object_t *object, 
                                           gconstpointer data, 
                                           gpointer userdata);
static guint xmms_sun_buffersize_get (xmms_output_t *output);
static guint xmms_sun_format_set (xmms_output_t *output, 
                                  xmms_audio_format_t *format);
static gboolean xmms_sun_open (xmms_output_t *output);
static gboolean xmms_sun_new (xmms_output_t *output);
static void xmms_sun_destroy (xmms_output_t *output);
static gboolean xmms_sun_mixer_set (xmms_output_t *output, gint left, 
                                    gint right);
static gboolean xmms_sun_mixer_get (xmms_output_t *output, gint *left, 
                                    gint *right);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, 
	                          XMMS_OUTPUT_PLUGIN_API_VERSION,
	                          "sun", "SUN Output", XMMS_VERSION,
	                          "OpenBSD SUN architecture output plugin");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.nittionio.nu/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	xmms_plugin_info_add (plugin, "E-Mail", "nano@nittionino.nu");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, 
	                        xmms_sun_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, 
	                        xmms_sun_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, 
	                        xmms_sun_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY,
	                        xmms_sun_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, 
	                        xmms_sun_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FORMAT_SET,
	                        xmms_sun_format_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET,
	                        xmms_sun_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, 
	                        xmms_sun_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_GET,
	                        xmms_sun_mixer_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_SET,
	                        xmms_sun_mixer_set);

	xmms_plugin_config_property_register (plugin,
	                                      "device",
	                                      "/dev/audio",
	                                      NULL,
	                                      NULL);

	xmms_plugin_config_property_register (plugin,
	                                      "mixer",
	                                      "/dev/mixer",
	                                      NULL,
	                                      NULL);
	       
	xmms_plugin_config_property_register (plugin,
	                                      "volume",
	                                      "70/70",
	                                      NULL,
	                                      NULL);

	return (plugin);
}

/*
 * Member functions
 */


/**
 * New mekkodon
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
	
	XMMS_DBG ("XMMS_SUN_NEW"); 
	
	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_sun_data_t, 1);

	val = xmms_plugin_config_lookup ( xmms_output_plugin_get (output), "mixer");
	mixdev = xmms_config_property_get_string (val);
	
	data->mixerfd = open (mixdev, O_WRONLY);
	if (!data->mixerfd == -1)
		data->have_mixer = FALSE;
	else
		data->have_mixer = TRUE;
	
	XMMS_DBG ("mixer: %d", data->have_mixer);
	data->mixer_conf = xmms_plugin_config_lookup (
	                           xmms_output_plugin_get (output),
	                           "volume");

	xmms_config_property_callback_set (data->mixer_conf,
	                                   xmms_sun_mixer_config_changed,
	                                   (gpointer) output);
	
	xmms_output_private_data_set (output, data); 
	
	val = xmms_plugin_config_lookup (xmms_output_plugin_get (output), "device");
	dev = xmms_config_property_get_string (val);

	/* report all suported formats */
	if ((tmp_audio_fd = open (dev, O_WRONLY)) < 0) {
		return FALSE;
	}
	
	AUDIO_INITINFO(&info);

	for (i = 0; i < G_N_ELEMENTS(formats); i++) {
		
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
		} else {
			continue;
		}

		for (j = 0; j < G_N_ELEMENTS(rates); j++) {
			for (k = 1; k <= 2; k++) { 			/* 1 or 2 channels */

				info.play.sample_rate = rates[j];
				info.play.channels = k;

				if (ioctl (tmp_audio_fd, AUDIO_SETINFO, &info) == 0) {
					xmms_output_format_add(output, formats[i].xmms_fmt,
					                       info.play.channels, 
					                       info.play.sample_rate);
				}
			}
		}
	}
	close(tmp_audio_fd);

	return TRUE; 
}

/**
 * Free mekkodon
 */
static void
xmms_sun_destroy (xmms_output_t *output)
{
	xmms_sun_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->have_mixer) {
		close (data->mixerfd);
	}

	xmms_config_property_callback_remove (data->mixer_conf,
	                                      xmms_sun_mixer_config_changed);
	close (data->fd);
	g_free (data);
}

/** 
 * Open audio device and configure it to play a stream.
 *
 * @param output The outputstructure we are supposed to fill in with our 
 *               private sun-blessed data
 *
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
	g_return_val_if_fail (data, FALSE);
	
	val = xmms_plugin_config_lookup (xmms_output_plugin_get (output),
	                                 "device");
	
	if ((dev = xmms_config_property_get_string (val)) == NULL) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "/dev/audio";
	}
	
	XMMS_DBG ("Opening device: %s", dev);

	AUDIO_INITINFO (&info);

	if ((data->fd = open (dev,O_WRONLY)) < 0) {
		xmms_log_error ("%s: %s", dev, (gchar *)strerror (errno));
		return FALSE;
	}

	enc.index = 0;
	while (ioctl (data->fd, AUDIO_GETENC, &enc) == 0 && 
	       enc.encoding != AUDIO_ENCODING_SLINEAR_LE) {
		enc.index++;
	}

	/* set defaults */
	info.play.encoding = enc.encoding;
	info.play.precision = enc.precision;
	info.play.channels = 2;
	info.play.sample_rate = 44100;
	info.mode = AUMODE_PLAY;
	if (ioctl (data->fd, AUDIO_SETINFO, &info) < 0) {
		xmms_log_error ("%s: %s", dev, strerror (errno));
		return FALSE;
	}

#ifdef XMMS_OS_SOLARIS
	enqued_bytes = 0;
#endif

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
	
	XMMS_DBG("XMMS_SUN_CLOSE");
	
	if (close (data->fd) < 0) {
		xmms_log_error ("Unable to close device (%s)", strerror(errno));
	}
}

/**
 * Handles config change requests.
 * @param output xmms object
 * @param data the new volume values
 * @param userdata
 */
static void 
xmms_sun_mixer_config_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_sun_data_t *sun_data;
	guint left, right;
	xmms_output_t *output;
	const gchar *newval;
	
	XMMS_DBG ("XMMS_SUN_MIXER_CONFIG_CHANGED");	
	
	g_return_if_fail (userdata);
	output = userdata;
	g_return_if_fail (data);
	newval = data;
	sun_data = xmms_output_private_data_get (output);
	g_return_if_fail (sun_data);

	if (sun_data->have_mixer) {
		gint res = sscanf (data, "%u/%u", &left, &right);

		if (res == 0) {
			xmms_log_error ("Unable to change volume");
			return;
		}

		if (res == 1) {
			right = left;
		}

		xmms_sun_mixer_set (output, left, right);
	}
}


/**
 * Set format
 *
 * @param output The output structure.
 * @param format The to-be-set audio format (samplerate and -format, channels)
 *
 * @return TRUE or FALSE on error
 */
static guint
xmms_sun_format_set (xmms_output_t *output, xmms_audio_format_t *format)
{
	xmms_sun_data_t *data;
	audio_info_t info;
	gint i;
	
	XMMS_DBG ("XMMS_SUN_FORMAT_SET %d %d %d", format->format,
	          format->channels, format->samplerate);
	
	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);	

	AUDIO_INITINFO(&info);
	
	/* translate the sample format */
	for (i = 0; i < G_N_ELEMENTS(formats); i++) {
		if (formats[i].xmms_fmt == format->format) {
			info.play.encoding = formats[i].sun_encoding;
			info.play.precision = formats[i].sun_precision;
		}
	}

	info.play.sample_rate = format->samplerate;
	info.play.channels = format->channels;

	if (ioctl (data->fd, AUDIO_SETINFO, &info) < 0) {	
		XMMS_DBG ("unable to change the format");
		return FALSE;
	} else {
		/* update the private data struct */
		data->samplerate = info.play.sample_rate;
		data->channels = info.play.channels;
		data->precision = info.play.precision;
		data->encoding = info.play.encoding;
		xmms_output_private_data_set (output, data);
	}

	return TRUE;
}


/**
 * Change mixer parameters.
 *
 * @param output The output structure
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_sun_mixer_set (xmms_output_t *output, gint left, gint right) 
{
	xmms_sun_data_t *data;
	mixer_devinfo_t info;
	mixer_ctrl_t mixer;
	
	XMMS_DBG ("XMMS_SUN_MIXER_SET");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);
	
	if (!data->have_mixer) {
		return FALSE;
	}
	
	AUDIO_INITINFO(&info);
	
	for (info.index = 0; ioctl (data->mixerfd, 
	     AUDIO_MIXER_DEVINFO, &info) >= 0; info.index++) {
		if (!strcmp ("dac", info.label.name)) {
			mixer.dev = info.index;
			break;
		}
	}

	mixer.type = AUDIO_MIXER_VALUE;
	mixer.un.value.num_channels = 2;

	mixer.un.value.level[AUDIO_MIXER_LEVEL_LEFT] = (left * 100) / 255;
	mixer.un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = (right * 100) / 255;

	if (ioctl (data->mixerfd, AUDIO_MIXER_WRITE, &mixer) < 0) {
		xmms_log_error ("Unable to change volume (%s)", strerror (errno));
	}

	return TRUE;
}



/**
 * Get mixer parameters.
 *
 * @param output The output structure
 *
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_sun_mixer_get (xmms_output_t *output, gint *left, gint *right) 
{
	xmms_sun_data_t *data;
	mixer_devinfo_t info;
	mixer_ctrl_t mixer;

	XMMS_DBG ("XMMS_SUN_MIXER_GET");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!data->have_mixer) {
		return FALSE;
	}

	for (info.index = 0; ioctl (data->mixerfd, 
	     AUDIO_MIXER_DEVINFO, &info) >= 0; info.index++) {
		if (!strcmp (data->mixer_voldev, info.label.name)) {
			mixer.dev = info.index;
		}
	}

	mixer.type = AUDIO_MIXER_VALUE;
	mixer.un.value.num_channels = 2;

	if (ioctl (data->mixerfd, AUDIO_MIXER_READ, &mixer) < 0) {
		xmms_log_error ("apan tutar i skogen!!! mixerfel, 445, hah");
	}

	*left = (mixer.un.value.level[AUDIO_MIXER_LEVEL_LEFT] * 100) / 255;
	*right = (mixer.un.value.level[AUDIO_MIXER_LEVEL_LEFT] * 100) / 255;

	return TRUE;
}



/**
 * Get buffersize.
 *
 * @param output The output structure.
 * 
 * @return the current buffer size (bytes available in buffer) 
 * or 0 on failure.
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
		xmms_log_error ("AUDIO_GETINFO: %s", strerror (errno));
		return FALSE;
	}	
#if defined(XMMS_OS_OPENBSD) || defined(XMMS_OS_NETBSD)
	pending_bytes = info.play.seek * (info.play.precision / 8);
#else  // XMMS_OS_SOLARIS
	pending_bytes = enqued_bytes - (info.play.samples * info.play.precision / 8);
#endif
	return pending_bytes;
}


/**
 * Flush buffer.
 *
 * @param output The output structure
 */
static void
xmms_sun_flush (xmms_output_t *output) 
{
	xmms_sun_data_t *data;	

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG("XMMS_SUN_FLUSH");

	if (ioctl (data->fd, AUDIO_FLUSH, NULL) < 0) {
		xmms_log_error ("unable to flush buffer");
	}

#ifdef XMMS_OS_SOLARIS
	enqued_bytes = 0;
#endif
}


/**
 * Write buffer to the output device.
 *
 * @param output The output structure filled with sun data.
 * @param buffer Audio data to be written to audio device.
 * @param len The length of audio data.
 */
static void
xmms_sun_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_sun_data_t *data;
	static ssize_t done;
	static ssize_t n;

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len);

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);


	for (done = 0; len > done; ) {

		n = write (data->fd, buffer, len - done);
		if (n == -1) {
			if (errno == EINTR)
				continue;
			else
				break;
		}
		done += n;
#ifdef XMMS_OS_SOLARIS
		/* keep track of everything ever written/played. Needed for the
		 * xmms_sun_buffersize_get function 
		 */
		enqued_bytes += n;
#endif
	}
}
