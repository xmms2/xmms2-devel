/** @file alsa.c
 * Output plugin for the Advanced Linux Sound Architechture.
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
 * @todo Some nice deinit stuff if init of mixer or pcm fails.
 * @todo The first time xmms2 plays a tune, visualization data gets
 *       out of sync, lagged or something. This is _only_ the first time
 *       and is not affected by "xmms2 stop", wait for release, "xmms2 play".
 *       A really strange error, any ideas?
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
#define SND_CHANNELS       2
#define SND_FORMAT         SND_PCM_FORMAT_S16
#define SND_STREAM         SND_PCM_STREAM_PLAYBACK
#define BUFFER_TIME        500000
#define PERIOD_TIME        100000


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
} xmms_alsa_data_t;


/*
 * Function prototypes
 */
static void xmms_alsa_flush (xmms_output_t *output);
static void xmms_alsa_close (xmms_output_t *output);
static void xmms_alsa_write (xmms_output_t *output, gchar *buffer, gint len);
static void xmms_alsa_xrun_recover (xmms_alsa_data_t *output);
static void xmms_alsa_mixer_config_changed (xmms_object_t *object, 
											gconstpointer data, 
											gpointer userdata);
static guint xmms_alsa_samplerate_set (xmms_output_t *output, guint rate);
static guint xmms_alsa_buffer_bytes_get (xmms_output_t *output);
static gboolean xmms_alsa_open (xmms_output_t *output);
static gboolean xmms_alsa_new (xmms_output_t *output);
static gboolean xmms_alsa_set_hwparams (xmms_alsa_data_t *data); 
static gboolean xmms_alsa_mixer_set (xmms_output_t *output, gint left, 
									 gint right);
static gboolean xmms_alsa_mixer_get (xmms_output_t *output, gint *left, 
									 gint *right);
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
							  "Advanced Linux Sound Architecture \
							   output plugin");

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
							xmms_alsa_buffer_bytes_get);
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
									   "mixer_dev",
									   "hw:0",
									   NULL,
									   NULL);

	xmms_plugin_config_value_register (plugin,
									   "volume",
									   "70/70",
									   NULL,
									   NULL);

	return plugin;
}


/*
 * Member functions
 */


/**
 * Creates data for new plugin.
 * Called when ALSA is selected as active output
 * plugin. Initializes alsa data struct and reads
 * mixer values from configuration file.
 *
 * @param output The output structure
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_alsa_new (xmms_output_t *output) 
{
	xmms_alsa_data_t *data;
	xmms_plugin_t *plugin;
	xmms_config_value_t *volume;
	
	XMMS_DBG ("XMMS_ALSA_NEW"); 
	
	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_alsa_data_t, 1);

	plugin = xmms_output_plugin_get (output);
	volume = xmms_plugin_config_lookup (plugin, "volume");

	xmms_config_value_callback_set (volume,
									xmms_alsa_mixer_config_changed,
									(gpointer) output);
	
	xmms_output_private_data_set (output, data); 
	
	return TRUE; 
}


/** 
 * Open audio device.
 *
 * @param output The output structure filled with alsa data.  
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_alsa_open (xmms_output_t *output)
{
	xmms_alsa_data_t *data;
	const xmms_config_value_t *cv;
	const gchar *dev;
	gint err = 0;
	gint tmp = 0;
	
	XMMS_DBG ("XMMS_ALSA_OPEN");	

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	cv = xmms_plugin_config_lookup (xmms_output_plugin_get (output), "device");
	dev = xmms_config_value_string_get (cv);
	
	if (!dev) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "default";
	}

	XMMS_DBG ("Opening device: %s", dev);

	/* Open the device */
	err = snd_pcm_open (&(data->pcm), dev, SND_STREAM, 0);
	if (err < 0) {
		xmms_log_error ("Cannot open audio device (%s)", snd_strerror (-err));
		return FALSE;
	}

	tmp = snd_pcm_format_physical_width (SND_FORMAT);
	data->frame_size = (gint)(tmp * SND_CHANNELS) / 8;

	data->have_mixer = xmms_alsa_mixer_setup (output);
	
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
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	
	XMMS_DBG("XMMS_ALSA_CLOSE");
	
	if (data->mixer) {
		err = snd_mixer_close (data->mixer);
		if (err != 0) {
			xmms_log_error ("Unable to release mixer device. (%s)", 
							snd_strerror (-err));
		} else {
			XMMS_DBG ("mixer device closed.");
		}
	}

	/* Close device */
	err = snd_pcm_close (data->pcm);
	if (err != 0) { 
		xmms_log_error ("Audio device could not be released. (%s)",
						snd_strerror (-err));
	} else {
		XMMS_DBG ("audio device closed.");
	}

	/* reset rate */
	data->rate = 0;
}



/**
 * Setup hardware parameters.
 *
 * @param data Internal plugin structure.
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean 
xmms_alsa_set_hwparams (xmms_alsa_data_t *data) 
{
	gint err, tmp;
	gint requested_buffer_time = BUFFER_TIME;
	gint requested_period_time = PERIOD_TIME;

	g_return_val_if_fail (data, FALSE);

	XMMS_DBG ("XMMS_ALSA_SET_HWPARAMS");
	
	/* Setup all parameters to configuration space */
	err = snd_pcm_hw_params_any (data->pcm, data->hwparams);
	if (err < 0) {
		xmms_log_error ("cannot initialize hardware parameter structure (%s)", 
						snd_strerror (-err));
		return FALSE;
	}

	/* Set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access (data->pcm, data->hwparams, 
	                                    SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		xmms_log_error ("cannot set access type (%s)", snd_strerror (-err));
		return FALSE;
	}

	/* Set the sample format */
	err = snd_pcm_hw_params_set_format (data->pcm, data->hwparams, SND_FORMAT);
	if (err < 0) {
		xmms_log_error ("cannot set sample format (%s)", snd_strerror (-err));
		return FALSE;
	}

	/* Set the count of channels */
	err = snd_pcm_hw_params_set_channels (data->pcm, data->hwparams, 
	                                      SND_CHANNELS);
	if (err < 0) {
		xmms_log_error ("cannot set channel count (%s)", snd_strerror (-err));
		return FALSE;
	}
	
	/* Set the sample rate */
	tmp = data->rate;
	err = snd_pcm_hw_params_set_rate_near (data->pcm, data->hwparams, 
	                                       &data->rate, NULL);
	if (err < 0) {
		xmms_log_error ("cannot set sample rate (%s)\n", snd_strerror (-err));
		return FALSE;
	}

	XMMS_DBG ("Sample rate requested: %dhz, got: %dhz",
	          tmp, data->rate);

	tmp = requested_buffer_time;
	err = snd_pcm_hw_params_set_buffer_time_near (data->pcm, data->hwparams,
	                                              &requested_buffer_time, 
	                                              NULL);
	if (err < 0) {
		xmms_log_error ("Buffer time <= 0 (%s)", snd_strerror (-err));  
		return FALSE;
	}

	XMMS_DBG ("Buffer time requested: %dms, got: %dms",
	          tmp / 1000, requested_buffer_time / 1000);

	err = snd_pcm_hw_params_get_buffer_size (data->hwparams,
	                                         &data->buffer_size);
	if (err != 0) {
		xmms_log_error ("unable to get buffer size (%s)", snd_strerror (-err));
		return FALSE;
	}
	
	/* Set period time */
	tmp = requested_period_time;
	err = snd_pcm_hw_params_set_period_time_near (data->pcm, data->hwparams, 
	                                              &requested_period_time, 
	                                              NULL);
	if (err < 0) {
		xmms_log_error ("cannot set periods (%s)", snd_strerror (-err));
		return FALSE;
	}

	XMMS_DBG ("Period time requested: %dms, got: %dms",
	          tmp / 1000, requested_period_time / 1000);

	/* Put the hardware parameters into good use */
	err = snd_pcm_hw_params (data->pcm, data->hwparams);
	if (err < 0) {
		xmms_log_error ("cannot set hw parameters (%s), %d", 
						snd_strerror (-err), snd_pcm_state (data->pcm));
		return FALSE;
	}

	/* Prepare sound card for teh shit */
	err = snd_pcm_prepare (data->pcm);
	if (err < 0) {
		xmms_log_error ("cannot prepare audio interface for use (%s)", 
						snd_strerror (-err));
		return FALSE;
	}

	return TRUE;
}



/**
 * Setup mixer
 *
 * @param output The output struct containing alsa data. 
 * @return TRUE on success, else FALSE
 */
static gboolean 
xmms_alsa_mixer_setup (xmms_output_t *output)
{
	xmms_alsa_data_t *data;
	const xmms_config_value_t *cv;
	gchar *dev, *name;
	guint left, right;
	snd_mixer_selem_id_t *selem_id;
	long alsa_min_vol, alsa_max_vol;
	gint err, index;
	
	XMMS_DBG ("XMMS_ALSA_SETUP_MIXER");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);
	
	cv = xmms_plugin_config_lookup (xmms_output_plugin_get (output), 
									"mixer_dev");
	dev = (gchar *)xmms_config_value_string_get (cv);
	
	err = snd_mixer_open (&data->mixer, 0);
	if (err < 0) {
		xmms_log_error ("Failed to open empty mixer: %s", snd_strerror (-err));
		data->mixer = NULL;
		return FALSE;
	}

	err = snd_mixer_attach (data->mixer, dev);
	if (err < 0) {
		xmms_log_error ("Attaching to mixer %s failed: %s", dev, 
						snd_strerror(-err));
		snd_mixer_close (data->mixer);
		data->mixer = NULL;
		return FALSE;
	}   

	err = snd_mixer_selem_register (data->mixer, NULL, NULL);
	if (err < 0) {
		xmms_log_error ("Failed to register mixer: %s", snd_strerror (-err));
		snd_mixer_close (data->mixer);
		data->mixer = NULL;
		return FALSE;
	}

	err = snd_mixer_load (data->mixer);
	if (err < 0) {
		xmms_log_error ("Failed to load mixer: %s", snd_strerror (-err));
		snd_mixer_close (data->mixer);
		data->mixer = NULL;
		return FALSE;
	}       

	cv = xmms_plugin_config_lookup (xmms_output_plugin_get (output), "mixer");
	name = (gchar *)xmms_config_value_string_get (cv);

	index = 0;

	snd_mixer_selem_id_alloca (&selem_id);
	
	snd_mixer_selem_id_set_index (selem_id, index);
	snd_mixer_selem_id_set_name (selem_id, name);
	
	data->mixer_elem = snd_mixer_find_selem (data->mixer, selem_id);
	if (data->mixer_elem == NULL) {
		xmms_log_error ("Failed to find mixer element");
		snd_mixer_close (data->mixer);
		data->mixer = NULL;
		return FALSE;
	}
	
	snd_mixer_selem_get_playback_volume_range (data->mixer_elem, &alsa_min_vol,
											   &alsa_max_vol);
	snd_mixer_selem_set_playback_volume_range (data->mixer_elem, 0, 100);
	
	if (alsa_max_vol == 0) {
		snd_mixer_close (data->mixer);
		data->mixer = NULL;
		data->mixer_elem = NULL;
		return FALSE;
	}

	xmms_alsa_mixer_get (output, &left, &right);
	xmms_alsa_mixer_set (output, left * 100 / alsa_max_vol, 
						 right * 100 / alsa_max_vol);

	return TRUE;
}



/**
 * Handle new mixer settings.
 * Extract left and right data and apply changes to the mixer.
 *
 * @param object An xmms object. 
 * @param data The new volume values.
 * @param userdata The output struct containing alsa data. 
 */
static void 
xmms_alsa_mixer_config_changed (xmms_object_t *object, gconstpointer data, 
								gpointer userdata)
{
	xmms_alsa_data_t *alsa_data;
	guint left, right, res;
	
	XMMS_DBG ("XMMS_ALSA_MIXER_CONFIG_CHANGED");	
	
	g_return_if_fail (data);
	g_return_if_fail (userdata);
	alsa_data = xmms_output_private_data_get (userdata);
	g_return_if_fail (alsa_data);

	if (alsa_data->have_mixer) {
		res = sscanf (data, "%u/%u", &left, &right);

		if (res == 0) {
			xmms_log_error ("Unable to change volume");
			return;
		}
		if (res == 1) {
			right = left; 
		}
		xmms_alsa_mixer_set (userdata, left, right);
	}
}



/**
 * Set sample rate.
 * Drain the buffer if non-empty and then try to change samplerate. 
 *
 * @param output The output struct containing alsa data. 
 * @param rate The to-be-set samplerate.
 *
 * @return The new samplerate or 0 on error.
 */
static guint
xmms_alsa_samplerate_set (xmms_output_t *output, guint rate)
{
	gint err;
	xmms_alsa_data_t *data;
	
	XMMS_DBG ("XMMS_ALSA_SAMPLERATE_SET");
	
	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);	

	/* Do we need to change our current rate? */
	if (data->rate != rate) {
		data->rate = rate;
		
		/* Get rid of old cow if any */
		if (snd_pcm_state (data->pcm) == SND_PCM_STATE_RUNNING) {
			err = snd_pcm_drain (data->pcm);	
			XMMS_DBG ("did we drain? --> %s", snd_strerror (-err));
		}
		
		/* Set new sample rate */
		snd_pcm_hw_params_alloca (&(data->hwparams)); 
		if (!xmms_alsa_set_hwparams (data)) {
			xmms_log_error ("Could not set hwparams, consult your local \
							guru for meditation courses");
			data->rate = 0;
		}
		xmms_output_private_data_set (output, data);
	}
	return data->rate;
}



/**
 * Change mixer settings.
 *
 * @param output The output struct containing alsa data. 
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_alsa_mixer_set (xmms_output_t *output, gint left, gint right) 
{
	xmms_alsa_data_t *data;

	XMMS_DBG ("XMMS_ALSA_MIXER_SET");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);
	
	if (!data->have_mixer) {
		return FALSE;
	}

	if (!data->mixer_elem) {
		xmms_log_error ("Mixer element not set!");
		return FALSE;
	}

	snd_mixer_selem_set_playback_volume (data->mixer_elem,
										 SND_MIXER_SCHN_FRONT_LEFT, 
										 left);
	
	snd_mixer_selem_set_playback_volume (data->mixer_elem,
										 SND_MIXER_SCHN_FRONT_RIGHT, 
										 right);
	
	return TRUE;
}



/**
 * Get mixer settings.
 *
 * @param output The output struct containing alsa data. 
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_alsa_mixer_get (xmms_output_t *output, gint *left, gint *right) 
{
	gint err;
	xmms_alsa_data_t *data;

	XMMS_DBG ("XMMS_ALSA_MIXER_GET");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!data->have_mixer) {
		return FALSE;
	}

	if (!data->mixer_elem) {
		xmms_log_error ("Mixer element not set!");
		return FALSE;
	}

	err = snd_mixer_handle_events (data->mixer);
	if (err != 0) {
		xmms_log_error ("Handling of pending mixer events failed (%s)",
						snd_strerror (-err));
		return FALSE;
	}

	snd_mixer_selem_get_playback_volume (data->mixer_elem,
										 SND_MIXER_SCHN_FRONT_LEFT,
										 (long int *)left);

	snd_mixer_selem_get_playback_volume (data->mixer_elem,
										 SND_MIXER_SCHN_FRONT_RIGHT,
										 (long int *)right);
	return TRUE;
}



/**
 * Get bytes in buffer.
 * Calculates bytes in buffer by subtract buffer size with available frames 
 * in buffer and then convert it to bytes. This is needed for the visualization 
 * to perform correct synchronization between audio and graphics.
 *
 * @param output The output struct containing alsa data. 
 * @return The current buffer size or 0 on failure.
 */
static guint
xmms_alsa_buffer_bytes_get (xmms_output_t *output) 
{
	gint bytes_in_buffer;
	xmms_alsa_data_t *data;
	snd_pcm_sframes_t avail;
	
	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	/* Get number of available frames in buffer */
	avail = snd_pcm_avail_update (data->pcm);
	if (avail == -EPIPE) {
		/* Spank alsa and give it another try */
		xmms_alsa_xrun_recover (data);
		avail = snd_pcm_avail_update (data->pcm);
		if (avail == -EPIPE) {
			xmms_log_error ("Unable to get available frames in buffer (%s)", 
					  snd_strerror (-avail));		
			return 0;
		}
	}
	
	bytes_in_buffer = snd_pcm_frames_to_bytes (data->pcm, data->buffer_size) - 
					  snd_pcm_frames_to_bytes (data->pcm, avail);

	return bytes_in_buffer;
}



/**
 * Flush buffer.
 * Resets the PCM position so that no data is left behind.
 *
 * @param output The output struct containing alsa data. 
 */
static void
xmms_alsa_flush (xmms_output_t *output) 
{
	gint err;
	xmms_alsa_data_t *data;	

	XMMS_DBG ("XMMS_ALSA_FLUSH");
	
	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	
	err = snd_pcm_reset (data->pcm);
	if (err != 0) {
		xmms_log_error ("Flush failed (%s)", snd_strerror (-err));
	}
}



/**
 * XRUN recovery.
 * If stuff gets messy (audio buffer cross-run), xrun recovery will 
 * perform some healthy clean up.
 *
 * @param data The private plugin data. 
 */
static void
xmms_alsa_xrun_recover (xmms_alsa_data_t *data)
{
	gint err;
	
	XMMS_DBG ("XMMS_ALSA_XRUN_RECOVER");
	
	g_return_if_fail (data);

	if (snd_pcm_state (data->pcm) == SND_PCM_STATE_XRUN) {
		err = snd_pcm_prepare (data->pcm);
		if (err < 0) {
			xmms_log_error ("xrun: prepare error, %s", snd_strerror (-err));
		}
	}
}



/**
 * Write buffer to the audio device.
 *
 * @param output The output struct containing alsa data. 
 * @param buffer Audio data to be written to audio device.
 * @param len The length of audio data.
 */
static void
xmms_alsa_write (xmms_output_t *output, gchar *buffer, gint len)
{
	gint written;
	gint written_frames;
	xmms_alsa_data_t *data;

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	while (len > 0) {
		written_frames = snd_pcm_writei (data->pcm, buffer, 
										 len / data->frame_size);
		if (written_frames > 0) {
			written = written_frames * data->frame_size;
			len -= written;
			buffer += written;
		}
		else if (written_frames == -EAGAIN || (written_frames > 0 
				 && written_frames < (len / data->frame_size))) {
			snd_pcm_wait (data->pcm, 100);
		}
		else if (written_frames == -EPIPE) {
			xmms_alsa_xrun_recover (data);
		}
		else {
			/* this will probably never happen */
			xmms_log_error ("Unknown error occured, report to maintainer: (%s)",
							snd_strerror (-written_frames));
		}
	}
}
