/** @file sun.c 
 *  Output plugin for the OpenBSD SUN architecture. 
 * 
 *  Copyright (C) 2003  Daniel Svensson, <nano@nittioonio.nu>
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
 *  @todo ungay xmms_sun_buffersize_get
 */

#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/signal_xmms.h"

#include <glib.h>

#include <sys/ioctl.h>
#include <sys/audioio.h>

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <stdlib.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
/*
 *  Defines
 */
#define SUN_DEFAULT_BLOCKSIZE 		8800
#define SUN_DEFAULT_BUFFER_SIZE 	8800
#define SUN_MIN_BUFFER_SIZE 		14336
#define SUN_DEFAULT_PREBUFFER_SIZE 	25
#define SUN_VERSION 				"0.6"

/*
 * Type definitions
 */

typedef struct xmms_sun_data_St {
	gchar *devaudio;
	gchar *devaudioctl;

	gint fd;
	gint mixerfd;

	gboolean have_mixer;

	gint req_pre_buffer_size;
	gint req_buffer_size;

	guint rate;
	xmms_config_value_t *mixer_conf;
	gchar *mixer_voldev;
} xmms_sun_data_t;


/*
 * Function prototypes
 */
static void xmms_sun_flush (xmms_output_t *output);
static void xmms_sun_close (xmms_output_t *output);
static void xmms_sun_write (xmms_output_t *output, gchar *buffer, gint len);
static void xmms_sun_mixer_config_changed (xmms_object_t *object, 
										   gconstpointer data, 
										   gpointer userdata);
static guint xmms_sun_samplerate_set (xmms_output_t *output, guint rate);
static guint xmms_sun_buffersize_get (xmms_output_t *output);
static gboolean xmms_sun_open (xmms_output_t *output);
static gboolean xmms_sun_new (xmms_output_t *output);
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

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "sun",
			"SUN Output" XMMS_VERSION,
			"OpenBSD SUN architecture output plugin");

	xmms_plugin_info_add (plugin, "URL", "http://www.nittionio.nu/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	xmms_plugin_info_add (plugin, "E-Mail", "nano@nittionino.nu");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, 
							xmms_sun_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, 
							xmms_sun_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, 
							xmms_sun_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, 
							xmms_sun_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, 
							xmms_sun_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, 
							xmms_sun_samplerate_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET,
							xmms_sun_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_GET,
							xmms_sun_mixer_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_SET,
							xmms_sun_mixer_set);


	xmms_plugin_config_value_register (plugin,
			"device",
			"/dev/audio",
			NULL,
			NULL);

	xmms_plugin_config_value_register (plugin,
			"mixer",
			"/dev/mixer",
			NULL,
			NULL);                                                                                                                                                  
	       
	xmms_plugin_config_value_register (plugin,
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
	const xmms_config_value_t *val;
	const gchar *dev;
	
	XMMS_DBG ("XMMS_SUN_NEW"); 
	
	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_sun_data_t, 1);

	val = xmms_plugin_config_lookup (
			xmms_output_plugin_get (output), "mixer");
	dev = xmms_config_value_string_get (val);
	
	data->mixerfd = open (dev, O_WRONLY);
	if (!data->mixerfd == -1)
		data->have_mixer = FALSE;
	else
		data->have_mixer = TRUE;
	
	XMMS_DBG ("mixer: %d", data->have_mixer);
	data->mixer_conf = xmms_plugin_config_lookup (
			xmms_output_plugin_get (output), "volume");

	xmms_config_value_callback_set (data->mixer_conf,
			xmms_sun_mixer_config_changed,
			(gpointer) output);
	
	xmms_output_private_data_set (output, data); 
	
	return TRUE; 
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
	const xmms_config_value_t *val;
	const gchar *dev;
	
	XMMS_DBG ("XMMS_SUN_OPEN");	

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);
	
	val = xmms_plugin_config_lookup (xmms_output_plugin_get (output), "device");
	
	if ((dev = xmms_config_value_string_get (val)) == NULL) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "/dev/audio";
	}
	
	XMMS_DBG ("Opening device: %s", dev);

	AUDIO_INITINFO (&info);

	if ((data->fd = open (dev,O_WRONLY)) < 0) {
		xmms_log_error ("%s: %s", dev, (gchar *)strerror (errno));
		return FALSE;

	}
	info.mode = AUMODE_PLAY;
	if (ioctl (data->fd, AUDIO_SETINFO, &info) != 0) {
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
	if (ioctl (data->fd, AUDIO_SETINFO, &info) != 0) {
		xmms_log_error ("%s: %s", dev, strerror (errno));
		return FALSE;
	}
	
	info.play.channels = 2;
	if (ioctl (data->fd, AUDIO_SETINFO, &info) < 0) {
		xmms_log_error ("%s: %s", dev, strerror (errno));
		return FALSE;
	}

	info.play.sample_rate = 44100;
	if (ioctl (data->fd, AUDIO_SETINFO, &info) < 0) {
		xmms_log_error ("%s: %s", dev, strerror (errno));
		return FALSE;
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
	
	XMMS_DBG("XMMS_SUN_CLOSE");
	
	if (close (data->fd) < 0) {
		xmms_log_error ("Unable to close device (%s)", strerror(errno));
	}
	data->rate = 0;
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
	guint left, right, err;
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
		res = sscanf (data, "%u/%u", &left, &right);
	
		if (res == 0) {
			xmms_log_error ("Unable to change volume");
			return;                                                             
						        
		if (res == 1) {
			right = left;
		}
		
		xmms_sun_mixer_set (output, left, right);
	}
}



/**
 * Set sample rate.
 *
 * @param output The output structure.
 * @param rate The to-be-set sample rate.
 *
 * @return the new sample rate or 0 on error
 */
static guint
xmms_sun_samplerate_set (xmms_output_t *output, guint rate)
{
	xmms_sun_data_t *data;
	audio_info_t info;
	
	XMMS_DBG ("XMMS_SUN_SAMPLERATE_SET");
	
	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);	

	AUDIO_INITINFO(&info);
	/* Do we need to change our current rate? */
	if (data->rate != rate) {
		
		info.play.sample_rate = rate;
		if (ioctl (data->fd, AUDIO_SETINFO, &info) < 0) {	
			XMMS_DBG ("unable to change samplerate");
			return FALSE;
		} else {
			data->rate = rate;
			xmms_output_private_data_set (output, data);
		}
	}
	return data->rate;
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
 * TODO: This is fucked up, please help me.
 *
 * @param output The output structure.
 * 
 * @return the current buffer size or 0 on failure.
 */
static guint
xmms_sun_buffersize_get (xmms_output_t *output) 
{
	xmms_sun_data_t *data;
	audio_offset_t offset;
	audio_info_t info;
	
	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	AUDIO_INITINFO (&info);
	if (ioctl (data->fd, AUDIO_GETINFO, &info) < 0) {
		xmms_log_error ("unable to get audio info.");
		return FALSE;
	}	

	if (ioctl (data->fd, AUDIO_GETOOFFS, &offset) < 0) {
		xmms_log_error ("unable to get buffer offset.");
		return FALSE; 
	} 

	//XMMS_DBG ("offset: %d", offset.offset);
	return 0;
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
    }
}
