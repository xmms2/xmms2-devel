/** @file alsa.c
 *  Output plugin for the Advanced Linux Sound Architechture.
 *  Known to work with kernel 2.6.
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
 *  @todo ungay xmms_alsa_buffersize_get
 */

#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/signal_xmms.h"

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include <glib.h>


/*
 *  Defines
 */
#define SND_CHANNELS     		2
#define SND_FORMAT       		SND_PCM_FORMAT_S16_LE
#define SND_STREAM       		SND_PCM_STREAM_PLAYBACK


/*
 * Type definitions
 */
typedef struct xmms_alsa_data_St {
	snd_pcm_t *pcm;
	snd_mixer_t *mixer;
	snd_mixer_elem_t *mixer_elem;
	snd_pcm_hw_params_t *hwparams;
 	snd_pcm_uframes_t  buffer_size;
	guint frame_size;
	guint rate;
	gboolean have_mixer;
	xmms_config_value_t *mixer_conf;
} xmms_alsa_data_t;


/*
 * Function prototypes
 */
static void xmms_alsa_flush (xmms_output_t *output);
static void xmms_alsa_close (xmms_output_t *output);
static void xmms_alsa_write (xmms_output_t *output, gchar *buffer, gint len);
static void xmms_alsa_xrun_recover (xmms_output_t *output);
static void xmms_alsa_mixer_config_changed (xmms_object_t *object, gconstpointer data, gpointer userdata);
static guint xmms_alsa_samplerate_set (xmms_output_t *output, guint rate);
static guint xmms_alsa_buffersize_get (xmms_output_t *output);
static gboolean xmms_alsa_open (xmms_output_t *output);
static gboolean xmms_alsa_new (xmms_output_t *output);
static gboolean xmms_alsa_set_hwparams (xmms_alsa_data_t *data); 
static gboolean xmms_alsa_mixer_set (xmms_output_t *output, gint left, gint right);
static gboolean xmms_alsa_mixer_get (xmms_output_t *output, gint *left, gint *right);
static gboolean xmms_alsa_mixer_setup (xmms_output_t *output);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "alsa",
			"ALSA Output" XMMS_VERSION,
			"Advanced Linux Sound Architecture output plugin");

	xmms_plugin_info_add (plugin, "URL", "http://www.nittionio.nu/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	xmms_plugin_info_add (plugin, "E-Mail", "nano@nittionino.nu");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, 
							xmms_alsa_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, 
							xmms_alsa_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, 
							xmms_alsa_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, 
							xmms_alsa_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, 
							xmms_alsa_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, 
							xmms_alsa_samplerate_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET,
							xmms_alsa_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_GET,
							xmms_alsa_mixer_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_SET,
							xmms_alsa_mixer_set);


	xmms_plugin_config_value_register (plugin,
			"device",
			"default",
			NULL,
			NULL);

	xmms_plugin_config_value_register (plugin,
			"mixer",
			"PCM",
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
xmms_alsa_new (xmms_output_t *output) 
{
	xmms_alsa_data_t *data;
	
	XMMS_DBG ("XMMS_ALSA_NEW"); 
	
	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_alsa_data_t, 1);

	data->mixer_conf = xmms_plugin_config_lookup (
			xmms_output_plugin_get (output), "volume");

	xmms_config_value_callback_set (data->mixer_conf,
			xmms_alsa_mixer_config_changed,
			(gpointer) output);
	
	xmms_output_plugin_data_set (output, data); 
	
	return TRUE; 
}


/** 
 * Open audio device and configure it to play a stream.
 *
 * @param output The outputstructure we are supposed to fill in with our 
 *               private alsa-blessed data
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_alsa_open (xmms_output_t *output)
{
	xmms_alsa_data_t *data;
	const xmms_config_value_t *val;
	const gchar *dev;
	gint err;
	gint alsa_bits_per_sample;
	
	XMMS_DBG ("XMMS_ALSA_OPEN");	

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, FALSE);

	val = xmms_plugin_config_lookup (xmms_output_plugin_get (output), "device");
	dev = xmms_config_value_string_get (val);
	
	if (!dev) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "default";
	}

	XMMS_DBG ("Opening device: %s", dev);

	/* Open the device */
	if ((err = snd_pcm_open (&(data->pcm), dev, SND_STREAM, 0)) < 0) {
		xmms_log_fatal ("FATAL: %s: cannot open audio device (%s)", 
				__FILE__, snd_strerror (-err));
		return FALSE;
	}

	alsa_bits_per_sample = snd_pcm_format_physical_width (SND_FORMAT);
	data->frame_size = (gint)(alsa_bits_per_sample * SND_CHANNELS) / 8;

	data->have_mixer = xmms_alsa_mixer_setup (output);
	
	XMMS_DBG ("bps: %d, frame size: %d", alsa_bits_per_sample, data->frame_size);
	
	return TRUE;
}



/**
 * Close audio device.
 *
 * @param output The output structure filled with alsa data.  
 */
static void
xmms_alsa_close (xmms_output_t *output)
{
	gint err;
	xmms_alsa_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);
	
	XMMS_DBG("XMMS_ALSA_CLOSE");
	
	if (data->mixer) {
		if ((err = snd_mixer_close (data->mixer)) != 0) {
			xmms_log_fatal ("Unable to release mixer device. (%s)", 
					snd_strerror (err));
		} else {
			XMMS_DBG ("mixer device closed.");
		}
	}

	/* Close device */
	if ((err = snd_pcm_close (data->pcm)) != 0) { 
		xmms_log_fatal ("Audio device could not be released. (%s)",
				snd_strerror (err));
	} else {
		XMMS_DBG ("audio device closed.");
	}

	/* reset rate */
	data->rate = 0;
}



/**
 * Setup hardware parameters.
 *
 * @param data Internal plugin structure
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean 
xmms_alsa_set_hwparams (xmms_alsa_data_t *data) 
{
	gint err;
	gint dir;
	gint requested_buffer_time;
	gint requested_period_time;

	g_return_val_if_fail (data, 0);

	XMMS_DBG ("XMMS_ALSA_SET_HWPARAMS");
	
	/* Setup all parameters to configuration space */
	if ((err = snd_pcm_hw_params_any (data->pcm, data->hwparams)) < 0) {
		xmms_log_fatal ("cannot initialize hardware parameter structure (%s)", 
				snd_strerror (err));
		return FALSE;
	}

	/* Set the interleaved read/write format */
	if ((err = snd_pcm_hw_params_set_access (data->pcm, 
					data->hwparams, 
					SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		xmms_log_fatal ("cannot set access type (%s)", snd_strerror (err));
		return FALSE;
	}

	/* Set the sample format */
	if ((err = snd_pcm_hw_params_set_format (data->pcm, data->hwparams, 
					SND_FORMAT)) < 0) {
		xmms_log_fatal ("cannot set sample format (%s)", snd_strerror (err));
		return FALSE;
	}

	/* Set the count of channels */
	if ((err = snd_pcm_hw_params_set_channels (data->pcm, data->hwparams, 
					SND_CHANNELS)) < 0) {
		xmms_log_fatal ("cannot set channel count (%s)", snd_strerror (err));
		return FALSE;
	}
	
	/* Set the sample rate */
	if ((err = snd_pcm_hw_params_set_rate_near (data->pcm, data->hwparams, 
					&data->rate, 0)) < 0) {
		xmms_log_fatal ("cannot set sample rate (%s)\n", snd_strerror (err));
		return FALSE;
	}

	/* Extract the rate we got */
	snd_pcm_hw_params_get_rate (data->hwparams, &err, 0);
	XMMS_DBG ("rate: %d", err);
	
	requested_buffer_time = 500000; 
	requested_period_time = 100000;
	
	if ((err = snd_pcm_hw_params_set_buffer_time_near (data->pcm, 
					data->hwparams, &requested_buffer_time, &dir)) < 0) {
		xmms_log_fatal ("Buffer time <= 0 (%s)", snd_strerror (-err));  
		return FALSE;
	}

	XMMS_DBG ("Buffer time requested: 500ms, got: %dms", 
			requested_buffer_time / 1000); 

	/*
	if ((err = snd_pcm_hw_params_get_buffer_size (data->hwparams, &data->buffer_size)) != 0) {
		xmms_log_fatal ("unable to get buffer size (%s)", snd_strerror (-err));
		return FALSE;
	}	
	*/
	
	/* Set period time */
	if ((err = snd_pcm_hw_params_set_period_time_near (data->pcm, 
					data->hwparams, &requested_period_time, &dir)) < 0) {
		xmms_log_fatal ("cannot set periods (%s)", snd_strerror (-err));
		return FALSE;
	}

	XMMS_DBG ("Period time requested: 100ms, got: %dms", 
			requested_period_time / 1000); 

	/* Put the hardware parameters into good use */
	if ((err = snd_pcm_hw_params (data->pcm, data->hwparams)) < 0) {
		xmms_log_fatal ("cannot set hw parameters (%s), %d", 
				snd_strerror (err), snd_pcm_state (data->pcm));

		return FALSE;
	}

	/* Prepare sound card for teh shit */
	if ((err = snd_pcm_prepare (data->pcm)) < 0) {
		xmms_log_fatal ("cannot prepare audio interface for use (%s)", 
				snd_strerror (err));
		return FALSE;
	}

	return TRUE;
}



/**
 * Setup mixer
 * @param output xmms output struct
 * @return TRUE on success, else FALSE
 */
static gboolean 
xmms_alsa_mixer_setup (xmms_output_t *output)
{
	xmms_alsa_data_t *data;
	gchar *dev, *name;
	guint left, right;
	snd_mixer_selem_id_t *selem_id;
	long alsa_min_vol, alsa_max_vol;
	gint err, index;

	
	XMMS_DBG ("XMMS_ALSA_SETUP_MIXER");
	g_return_val_if_fail (output, FALSE);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, FALSE);
	
	dev = "hw:0"; 
	
	if ((err = snd_mixer_open (&data->mixer, 0)) < 0) {
		xmms_log_fatal ("Failed to open empty mixer: %s", snd_strerror (-err));
		data->mixer = NULL;
		return FALSE;
	}
	if ((err = snd_mixer_attach (data->mixer, dev)) < 0)	{
		xmms_log_fatal ("Attaching to mixer %s failed: %s", dev, 
				snd_strerror(-err));
		return FALSE;
	}   
	if ((err = snd_mixer_selem_register (data->mixer, NULL, NULL)) < 0) {       
		xmms_log_fatal ("Failed to register mixer: %s", snd_strerror (-err));
		return FALSE;
	}
	if ((err = snd_mixer_load (data->mixer)) < 0) {
		xmms_log_fatal ("Failed to load mixer: %s", snd_strerror (-err));
		return FALSE;
	}       
	
	name = "PCM";
	index = 0;

	snd_mixer_selem_id_alloca (&selem_id);
	
	snd_mixer_selem_id_set_index (selem_id, index);
	snd_mixer_selem_id_set_name (selem_id, name);
	XMMS_DBG ("XMMS_ALSA_SETUP_MIXER - selem-mekket");
	
	if ((data->mixer_elem = snd_mixer_find_selem (data->mixer, selem_id)) == NULL) {
		xmms_log_fatal ("Failed to find mixer element");
		return FALSE;
	}
	
	snd_mixer_selem_get_playback_volume_range (data->mixer_elem, 
			&alsa_min_vol, &alsa_max_vol);
	snd_mixer_selem_set_playback_volume_range(data->mixer_elem, 
			0, 100);
	
	if (alsa_max_vol == 0) {
		data->mixer_elem = NULL;
		return FALSE;
	}

	xmms_alsa_mixer_get (output, &left, &right);
	xmms_alsa_mixer_set (output, left * 100 / alsa_max_vol, right* 100 / alsa_max_vol);

	return TRUE;
}



/**
 * Handles config change requests.
 * @param output xmms object
 * @param data the new volume values
 * @param userdata
 */
static void 
xmms_alsa_mixer_config_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_alsa_data_t *alsa_data;
	gchar **tmp;
	guint left, right;
	xmms_output_t *output;
	const gchar *newval;
	
	XMMS_DBG ("XMMS_ALSA_MIXER_CONFIG_CHANGED");	
	
	g_return_if_fail (userdata);
	output = userdata;
	g_return_if_fail (data);
	newval = data;
	alsa_data = xmms_output_plugin_data_get (output);
	g_return_if_fail (alsa_data);

	if (alsa_data->have_mixer) {
		tmp = g_strsplit (newval, "/", 2);

		if (tmp[0]) {
			left  = atoi (tmp[0]);
		}

		if (tmp[1]) {
			right = atoi (tmp[1]);
		} else {
			right = left;
		}
		
		xmms_alsa_mixer_set (output, left, right);
		
		g_strfreev (tmp);
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
xmms_alsa_samplerate_set (xmms_output_t *output, guint rate)
{
	xmms_alsa_data_t *data;
	gint err;
	
	XMMS_DBG ("XMMS_ALSA_SAMPLERATE_SET");
	XMMS_DBG ("trying to change rate to: %d", rate);
	
	g_return_val_if_fail (output, 0);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, 0);	

	/* Do we need to change our current rate? */
	if (data->rate != rate) {
		data->rate = rate;
		xmms_output_plugin_data_set (output, data);
		
		/* Get rid of old cow if any */
		if (snd_pcm_state (data->pcm) == SND_PCM_STATE_RUNNING) {
			err = snd_pcm_drain (data->pcm);	
			XMMS_DBG ("did we drain? --> %s", snd_strerror (err));
		}
		
		/* Set new sample rate */
		snd_pcm_hw_params_alloca (&(data->hwparams)); 
		if (!xmms_alsa_set_hwparams (data)) {
			xmms_log_fatal ("Could not set hwparams, consult your local guro for \
					meditation courses\n");
			data->rate = 0;
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
xmms_alsa_mixer_set (xmms_output_t *output, gint left, gint right) 
{
	xmms_alsa_data_t *data;
	XMMS_DBG ("XMMS_ALSA_MIXER_SET");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, FALSE);
	
	if (!data->have_mixer) {
		return FALSE;
	}

	snd_mixer_selem_set_playback_volume(data->mixer_elem,
			SND_MIXER_SCHN_FRONT_LEFT, left);
	
	snd_mixer_selem_set_playback_volume(data->mixer_elem,
			SND_MIXER_SCHN_FRONT_RIGHT, right);
	
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
xmms_alsa_mixer_get (xmms_output_t *output, gint *left, gint *right) 
{
	xmms_alsa_data_t *data;

	XMMS_DBG ("XMMS_ALSA_MIXER_GET");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!data->have_mixer) {
		return FALSE;
	}

	snd_mixer_handle_events (data->mixer);
		
	snd_mixer_selem_get_playback_volume (data->mixer_elem,
			SND_MIXER_SCHN_FRONT_LEFT,
			(long int *)left);
	
	snd_mixer_selem_get_playback_volume (data->mixer_elem,
			SND_MIXER_SCHN_FRONT_RIGHT,
			(long int *)right);

	return TRUE;
	
}



/**
 * Get buffersize.
 *
 * @param output The output structure.
 * 
 * @return the current buffer size or 0 on failure.
 */
static guint
xmms_alsa_buffersize_get (xmms_output_t *output) 
{
	xmms_alsa_data_t *data;
	snd_pcm_sframes_t avail;
	
/*	XMMS_DBG ("XMMS_ALSA_BUFFERSIZE_GET"); */
	
	g_return_val_if_fail (output, 0);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, 0);

	/* Get number of available frames in buffer */
	if ((avail = snd_pcm_avail_update (data->pcm)) == -EPIPE) {

		/* Spank alsa and give it another try */
		xmms_alsa_xrun_recover (output);
		if ((avail = snd_pcm_avail_update (data->pcm)) == -EPIPE) {
			XMMS_DBG ("Unable to get number of frames in buffer (%s)", 
				snd_strerror (-avail));		
			return FALSE;
		}
	}
	return snd_pcm_frames_to_bytes (data->pcm, avail);

}



/**
 * Flush buffer.
 *
 * @param output The output structure
 */
static void
xmms_alsa_flush (xmms_output_t *output) 
{
	xmms_alsa_data_t *data;	
	gint err;

	g_return_if_fail (output);
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG("XMMS_ALSA_FLUSH");
	
	if ((err = snd_pcm_reset (data->pcm)) != 0) {
		XMMS_DBG ("Flush failed (%s)", snd_strerror (-err));
	}

}



/**
 * XRUN recovery.
 *
 * @param output The output structure filled with alsa data. 
 */
static void
xmms_alsa_xrun_recover (xmms_output_t *output)
{
	gint err;
	xmms_alsa_data_t *data;
	
	XMMS_DBG("XMMS_ALSA_XRUN_RECOVER");
	
	g_return_if_fail (output);
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	if (snd_pcm_state (data->pcm) == SND_PCM_STATE_XRUN) {
		if ((err = snd_pcm_prepare (data->pcm)) < 0) {
			xmms_log_fatal ("xrun: prepare error, %s", snd_strerror (err));
		}
	}
}



/**
 * Write buffer to the output device.
 *
 * @param output The output structure filled with alsa data.
 * @param buffer Audio data to be written to audio device.
 * @param len The length of audio data.
 */
static void
xmms_alsa_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_alsa_data_t *data;
	gint written;
	gint written_frames;

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len);
	
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	while (len > 0) {

		if ((written_frames = snd_pcm_writei (data->pcm, buffer, 
						len / data->frame_size)) > 0) {
			written = written_frames * data->frame_size;
			len -= written;
			buffer += written;
		}
		else if (written_frames == -EAGAIN || (written_frames > 0 
					&& written_frames < (len / data->frame_size))) {
			snd_pcm_wait (data->pcm, 100);
		}
		else if (written_frames == -EPIPE) {
			XMMS_DBG ("XRun detected");
			xmms_alsa_xrun_recover (output);
		}
		else {
			/* When? */
			XMMS_DBG ("Read/Write error: %d", written_frames);
		}
	}
}
