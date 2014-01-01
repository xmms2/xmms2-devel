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

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include <glib.h>

/*
 *  Defines
 */
#define BUFFER_TIME        500000
#define MAX_CHANNELS       8

/*
 * Type definitions
 */
typedef struct xmms_alsa_data_St {
	snd_pcm_t *pcm;
	snd_mixer_t *mixer;
	snd_mixer_elem_t *mixer_elem;
} xmms_alsa_data_t;

static const struct {
	xmms_sample_format_t xmms_fmt;
	snd_pcm_format_t alsa_fmt;
} formats[] = {
	{XMMS_SAMPLE_FORMAT_U8, SND_PCM_FORMAT_U8},
	{XMMS_SAMPLE_FORMAT_S8, SND_PCM_FORMAT_S8},
	{XMMS_SAMPLE_FORMAT_S16, SND_PCM_FORMAT_S16},
	{XMMS_SAMPLE_FORMAT_U16, SND_PCM_FORMAT_U16},
	{XMMS_SAMPLE_FORMAT_S32, SND_PCM_FORMAT_S32},
	{XMMS_SAMPLE_FORMAT_U32, SND_PCM_FORMAT_U32},
	{XMMS_SAMPLE_FORMAT_FLOAT, SND_PCM_FORMAT_FLOAT},
	{XMMS_SAMPLE_FORMAT_DOUBLE, SND_PCM_FORMAT_FLOAT64}
};

static const int rates[] = {
	8000,
	11025,
	12000,
	16000,
	22050,
	24000,
	32000,
	44100,
	48000,
	88200,
	96000,
	176400,
	192000
};

static const struct {
	snd_mixer_selem_channel_id_t id;
	const gchar *name;
} channel_map[] = {
	{SND_MIXER_SCHN_FRONT_LEFT, "left"},
	{SND_MIXER_SCHN_FRONT_RIGHT, "right"}
};

/*
 * Function prototypes
 */
static gboolean xmms_alsa_plugin_setup (xmms_output_plugin_t *plugin);
static void xmms_alsa_flush (xmms_output_t *output);
static void xmms_alsa_close (xmms_output_t *output);
static void xmms_alsa_write (xmms_output_t *output, gpointer buffer, gint len,
                             xmms_error_t *err);
static guint xmms_alsa_buffer_bytes_get (xmms_output_t *output);
static gboolean xmms_alsa_open (xmms_output_t *output);
static gboolean xmms_alsa_new (xmms_output_t *output);
static void xmms_alsa_destroy (xmms_output_t *output);
static gboolean xmms_alsa_format_set (xmms_output_t *output,
                                      const xmms_stream_type_t *format);
static gboolean xmms_alsa_set_hwparams (xmms_alsa_data_t *data,
                                        const xmms_stream_type_t *format);
static gboolean xmms_alsa_volume_set (xmms_output_t *output,
                                      const gchar *channel,
                                      guint volume);
static gboolean xmms_alsa_volume_get (xmms_output_t *output,
                                      const gchar **names, guint *values,
                                      guint *num_channels);
static gboolean xmms_alsa_mixer_setup (xmms_output_t *plugin,
                                       xmms_alsa_data_t *data);
static gboolean xmms_alsa_probe_modes (xmms_output_t *output,
                                       xmms_alsa_data_t *data);
static void xmms_alsa_probe_mode (xmms_output_t *output, snd_pcm_t *pcm,
                                  snd_pcm_format_t alsa_fmt,
                                  xmms_sample_format_t xmms_fmt,
                                  gint channels, gint rate);
static snd_mixer_elem_t *xmms_alsa_find_mixer_elem (snd_mixer_t *mixer,
                                                    gint index,
                                                    const char *name);

static snd_mixer_selem_channel_id_t lookup_channel (const gchar *name);

/*
 * Plugin header
 */
XMMS_OUTPUT_PLUGIN ("alsa", "ALSA Output", XMMS_VERSION,
                    "Advanced Linux Sound Architecture output plugin",
                    xmms_alsa_plugin_setup);

static gboolean
xmms_alsa_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_alsa_new;
	methods.destroy = xmms_alsa_destroy;

	methods.open = xmms_alsa_open;
	methods.close = xmms_alsa_close;

	methods.flush = xmms_alsa_flush;
	methods.format_set = xmms_alsa_format_set;

	methods.volume_get = xmms_alsa_volume_get;
	methods.volume_set = xmms_alsa_volume_set;

	methods.write = xmms_alsa_write;

	methods.latency_get = xmms_alsa_buffer_bytes_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	xmms_output_plugin_config_property_register (plugin, "device", "default",
	                                             NULL, NULL);

	xmms_output_plugin_config_property_register (plugin, "mixer", "PCM",
	                                             NULL, NULL);

	xmms_output_plugin_config_property_register (plugin, "mixer_dev", "default",
	                                             NULL,NULL);

	xmms_output_plugin_config_property_register (plugin, "mixer_index", "0",
	                                             NULL, NULL);

	return TRUE;
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

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_alsa_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	if (!xmms_alsa_probe_modes (output, data)) {
		g_free (data);
		return FALSE;
	}

	xmms_alsa_mixer_setup (output, data);

	xmms_output_private_data_set (output, data);

	return TRUE;
}

static gboolean
xmms_alsa_probe_modes (xmms_output_t *output, xmms_alsa_data_t *data)
{
	const xmms_config_property_t *cv;
	const gchar *dev;
	int i, j, k, err;

	cv = xmms_output_config_lookup (output, "device");
	dev = xmms_config_property_get_string (cv);

	if (!dev) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "default";
	}

	XMMS_DBG ("Probing device: %s", dev);

	err = snd_pcm_open (&(data->pcm), dev, SND_PCM_STREAM_PLAYBACK,
	                    SND_PCM_NONBLOCK);
	if (err < 0) {
		xmms_log_error ("Couldn't open device: %s", dev);
		return FALSE;
	}

	snd_pcm_nonblock (data->pcm, 0);

	for (i = 0; i < G_N_ELEMENTS (formats); i++) {
		for (j = 1; j <= MAX_CHANNELS; j++) {
			for (k = 0; k < G_N_ELEMENTS (rates); k++) {
				xmms_alsa_probe_mode (output, data->pcm,
				                      formats[i].alsa_fmt,
				                      formats[i].xmms_fmt, j, rates[k]);
			}
		}
	}

	snd_pcm_close (data->pcm);

	return TRUE;
}

static void
xmms_alsa_probe_mode (xmms_output_t *output, snd_pcm_t *pcm,
                      snd_pcm_format_t alsa_fmt,
                      xmms_sample_format_t xmms_fmt,
                      gint channels, gint rate)
{
	snd_pcm_hw_params_t *params;
	gint err;
	guint tmp;

	snd_pcm_hw_params_alloca (&params);

	/* Setup all parameters to configuration space */
	err = snd_pcm_hw_params_any (pcm, params);
	if (err < 0) {
		xmms_log_error ("Broken configuration for playback: no configurations "
		                "available: %s", snd_strerror (err));
		return;
	}

	err = snd_pcm_hw_params_set_rate_resample (pcm, params, 0);
	if (err < 0) {
		xmms_log_error ("Could not disable ALSA resampling, your CPU will burn.");
	}

	/* Set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access (pcm, params,
	                                    SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		xmms_log_error ("Access type not available for playback: %s",
		                snd_strerror (err));
		return;
	}

	err = snd_pcm_hw_params_set_format (pcm, params, alsa_fmt);
	if (err < 0) {
		xmms_log_error ("Sample format (%i) not available for playback.",
		                alsa_fmt);
		return;
	}

	err = snd_pcm_hw_params_set_channels (pcm, params, channels);
	if (err < 0) {
		xmms_log_error ("Channels count (%i) not available for playbacks.",
		                channels);
		return;
	}

	tmp = rate;
	err = snd_pcm_hw_params_set_rate_near (pcm, params, &tmp, NULL);
	if (err < 0) {
		xmms_log_error ("Rate %iHz not available for playback.", rate);
		return;
	}

	xmms_output_format_add (output, xmms_fmt, channels, tmp);
}

/**
 * Frees data for this plugin allocated in xmms_alsa_new().
 */
static void
xmms_alsa_destroy (xmms_output_t *output)
{
	xmms_alsa_data_t *data;
	gint err;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->mixer) {
		err = snd_mixer_close (data->mixer);
		if (err != 0) {
			xmms_log_error ("Unable to release mixer device: %s",
			                snd_strerror (err));
		} else {
			XMMS_DBG ("mixer device closed.");
		}
	}

	g_free (data);
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
	const xmms_config_property_t *cv;
	const gchar *dev;
	gint err = 0;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	cv = xmms_output_config_lookup (output, "device");
	dev = xmms_config_property_get_string (cv);

	if (!dev) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "default";
	}

	XMMS_DBG ("Opening device: %s", dev);

	/* Open the device */
	err = snd_pcm_open (&(data->pcm), dev, SND_PCM_STREAM_PLAYBACK,
	                    SND_PCM_NONBLOCK);
	if (err < 0) {
		xmms_log_error ("Cannot open audio device: %s", snd_strerror (err));
		return FALSE;
	}

	snd_pcm_nonblock (data->pcm, 0);

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

	/* Close device */
	err = snd_pcm_close (data->pcm);
	if (err != 0) {
		xmms_log_error ("Audio device could not be released: %s",
		                snd_strerror (err));
	} else {
		data->pcm = NULL;
		XMMS_DBG ("audio device closed.");
	}
}

/**
 * Setup hardware parameters.
 *
 * @param data Internal plugin structure.
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_alsa_set_hwparams (xmms_alsa_data_t *data,
                        const xmms_stream_type_t *format)
{
	snd_pcm_format_t alsa_format = SND_PCM_FORMAT_UNKNOWN;
	gint err, tmp, i, fmt;
	guint requested_buffer_time = BUFFER_TIME;
	snd_pcm_hw_params_t *hwparams;

	g_return_val_if_fail (data, FALSE);

	snd_pcm_hw_params_alloca (&hwparams);

	/* what alsa format does this format correspond to? */
	fmt = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_FORMAT);
	for (i = 0; i < G_N_ELEMENTS (formats); i++) {
		if (formats[i].xmms_fmt == fmt) {
			alsa_format = formats[i].alsa_fmt;
			break;
		}
	}

	g_return_val_if_fail (alsa_format != SND_PCM_FORMAT_UNKNOWN, FALSE);

	/* Setup all parameters to configuration space */
	err = snd_pcm_hw_params_any (data->pcm, hwparams);
	if (err < 0) {
		xmms_log_error ("Broken configuration for playback: no configurations "
		                "available: %s", snd_strerror (err));
		return FALSE;
	}

	/* Set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access (data->pcm, hwparams,
	                                    SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		xmms_log_error ("Access type not available for playback: %s",
		                snd_strerror (err));
		return FALSE;
	}

	/* Set the sample format */
	err = snd_pcm_hw_params_set_format (data->pcm, hwparams, alsa_format);
	if (err < 0) {
		xmms_log_error ("Sample format not available for playback: %s",
		                snd_strerror (err));
		return FALSE;
	}

	/* Set the count of channels */
	tmp = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_CHANNELS);
	err = snd_pcm_hw_params_set_channels (data->pcm, hwparams, tmp);
	if (err < 0) {
		xmms_log_error ("Channels count (%i) not available for playbacks: %s",
		                tmp, snd_strerror (err));
		return FALSE;
	}

	/* Set the sample rate.
	 * Note: don't use snd_pcm_hw_params_set_rate_near(), we want to fail here
	 *       if the core passed an unsupported samplerate to us!
	 */
	tmp = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	err = snd_pcm_hw_params_set_rate (data->pcm, hwparams, tmp, 0);
	if (err < 0) {
		xmms_log_error ("Rate %iHz not available for playback: %s",
		                tmp, snd_strerror (err));
		return FALSE;
	}

	tmp = requested_buffer_time;
	err = snd_pcm_hw_params_set_buffer_time_near (data->pcm, hwparams,
	                                              &requested_buffer_time, NULL);
	if (err < 0) {
		xmms_log_error ("Unable to set buffer time %i for playback: %s", tmp,
		                snd_strerror (err));
		return FALSE;
	}

	XMMS_DBG ("Buffer time requested: %dms, got: %dms",
	          tmp / 1000, requested_buffer_time / 1000);

	/* Put the hardware parameters into good use */
	err = snd_pcm_hw_params (data->pcm, hwparams);
	if (err < 0) {
		xmms_log_error ("Unable to set hw params for playback: %s",
		                snd_strerror (err));
		return FALSE;
	}

	return TRUE;
}



/**
 * Setup mixer
 *
 * @param plugin The output plugin
 * @param data The private plugin data.
 * @return TRUE on success, else FALSE
 */
static gboolean
xmms_alsa_mixer_setup (xmms_output_t *output, xmms_alsa_data_t *data)
{
	const xmms_config_property_t *cv;
	const gchar *dev, *name;
	glong alsa_min_vol = 0, alsa_max_vol = 0;
	gint index, err;

	cv = xmms_output_config_lookup (output, "mixer_dev");
	dev = xmms_config_property_get_string (cv);

	err = snd_mixer_open (&data->mixer, 0);
	if (err < 0) {
		xmms_log_error ("Failed to open empty mixer: %s", snd_strerror (err));
		data->mixer = NULL;

		return FALSE;
	}

	err = snd_mixer_attach (data->mixer, dev);
	if (err < 0) {
		xmms_log_error ("Attaching to mixer %s failed: %s", dev,
		                snd_strerror (err));
		snd_mixer_close (data->mixer);
		data->mixer = NULL;

		return FALSE;
	}

	err = snd_mixer_selem_register (data->mixer, NULL, NULL);
	if (err < 0) {
		xmms_log_error ("Failed to register mixer: %s", snd_strerror (err));
		snd_mixer_close (data->mixer);
		data->mixer = NULL;

		return FALSE;
	}

	err = snd_mixer_load (data->mixer);
	if (err < 0) {
		xmms_log_error ("Failed to load mixer: %s", snd_strerror (err));
		snd_mixer_close (data->mixer);
		data->mixer = NULL;

		return FALSE;
	}

	cv = xmms_output_config_lookup (output, "mixer");
	name = xmms_config_property_get_string (cv);

	cv = xmms_output_config_lookup (output, "mixer_index");
	index = xmms_config_property_get_int (cv);

	if (index < 0) {
		xmms_log_error ("mixer_index must not be negative; using 0.");
		index = 0;
	}

	data->mixer_elem = xmms_alsa_find_mixer_elem (data->mixer, index, name);
	if (!data->mixer_elem) {
		xmms_log_error ("Failed to find mixer element");
		snd_mixer_close (data->mixer);
		data->mixer = NULL;

		return FALSE;
	}

	snd_mixer_selem_get_playback_volume_range (data->mixer_elem,
	                                           &alsa_min_vol,
	                                           &alsa_max_vol);
	if (!alsa_max_vol) {
		snd_mixer_close (data->mixer);
		data->mixer = NULL;
		data->mixer_elem = NULL;

		return FALSE;
	}

	snd_mixer_selem_set_playback_volume_range (data->mixer_elem, 0, 100);

	return TRUE;
}

/**
 * Set audio format.
 * Drain the buffer if non-empty and then try to change audio format.
 *
 * @param output The output struct containing alsa data.
 * @param format The new audio format.
 *
 * @return Success/failure
 */
static gboolean
xmms_alsa_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	gint err = 0;
	xmms_alsa_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	switch (snd_pcm_state (data->pcm)) {
		case SND_PCM_STATE_OPEN:
		case SND_PCM_STATE_SETUP:
		case SND_PCM_STATE_PREPARED:
			break;
		case SND_PCM_STATE_XRUN:
			err = snd_pcm_recover (data->pcm, -ESTRPIPE, 0);
			break;
		case SND_PCM_STATE_SUSPENDED:
			err = snd_pcm_recover (data->pcm, -EPIPE, 0);
			break;
		case SND_PCM_STATE_RUNNING:
			err = snd_pcm_drain (data->pcm);
			break;
		default:
			XMMS_DBG ("Got unexpected PCM state: %d", snd_pcm_state (data->pcm));
			return FALSE;
	}

	if (err < 0) {
		xmms_log_error ("Unable to prepare PCM device new format: %s", snd_strerror (err));
		return FALSE;
	}

	/* Set new audio format*/
	if (!xmms_alsa_set_hwparams (data, format)) {
		xmms_log_error ("Could not set hwparams, consult your local "
		                "guru for meditation courses.");
		return FALSE;
	}

	return TRUE;
}

/**
 * Change mixer settings.
 *
 * @param output The output struct containing alsa data.
 * @param channel_name The name of the channel to set (e.g. "left").
 * @param volume The volume to set the channel to.
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_alsa_volume_set (xmms_output_t *output,
                      const gchar *channel_name, guint volume)
{
	xmms_alsa_data_t *data;
	gint channel, err;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (channel_name, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	g_return_val_if_fail (volume <= 100, FALSE);

	if (!data->mixer || !data->mixer_elem) {
		return FALSE;
	}

	channel = lookup_channel (channel_name);
	if (channel == SND_MIXER_SCHN_UNKNOWN) {
		return FALSE;
	}

	err = snd_mixer_selem_set_playback_volume (data->mixer_elem,
	                                           channel, volume);

	return (err >= 0);
}

/**
 * Get mixer settings.
 *
 * @param output The output struct containing alsa data.
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_alsa_volume_get (xmms_output_t *output, const gchar **names,
                      guint *values, guint *num_channels)
{
	xmms_alsa_data_t *data;
	gint i, err;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	g_return_val_if_fail (num_channels, FALSE);

	if (!data->mixer || !data->mixer_elem) {
		return FALSE;
	}

	if (!*num_channels) {
		*num_channels = G_N_ELEMENTS (channel_map);
		return TRUE;
	}

	g_return_val_if_fail (*num_channels == G_N_ELEMENTS (channel_map), FALSE);
	g_return_val_if_fail (names, FALSE);
	g_return_val_if_fail (values, FALSE);

	err = snd_mixer_handle_events (data->mixer);
	if (err < 0) {
		xmms_log_error ("Handling of pending mixer events failed: %s",
		                snd_strerror (err));
		return FALSE;
	}

	for (i = 0; i < *num_channels; i++) {
		glong tmp = 0;

		err = snd_mixer_selem_get_playback_volume (data->mixer_elem,
		                                           channel_map[i].id,
		                                           &tmp);
		if (err < 0) {
			continue;
		}

		/* safe, because we set the volume range to 0..100 */
		values[i] = tmp;
		names[i] = channel_map[i].name;
	}

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
	xmms_alsa_data_t *data;
	snd_pcm_sframes_t avail;
	gint ret;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	ret = snd_pcm_delay (data->pcm, &avail);
	if (ret != 0 || avail < 0) {
		return 0;
	}

	return snd_pcm_frames_to_bytes (data->pcm, avail);
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

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	err = snd_pcm_drop (data->pcm);
	if (err >= 0) {
		err = snd_pcm_prepare (data->pcm);
	}

	if (err < 0) {
		xmms_log_error ("Flush failed: %s", snd_strerror (err));
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
xmms_alsa_write (xmms_output_t *output, gpointer buffer, gint len,
                 xmms_error_t *error)
{
	gint written;
	gint frames;
	xmms_alsa_data_t *data;

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	g_return_if_fail (data->pcm);

	frames = snd_pcm_bytes_to_frames (data->pcm, len);

	while (frames > 0) {
		written = snd_pcm_writei (data->pcm, buffer, frames);

		if (written > 0) {
			frames -= written;
			buffer += snd_pcm_frames_to_bytes (data->pcm, written);
		} else if (written == -EAGAIN || written == -EINTR) {
			snd_pcm_wait (data->pcm, 100);
		} else if (written == -EPIPE || written == -ESTRPIPE) {
			if (snd_pcm_recover (data->pcm, written, 0) < 0) {
				gchar *message = g_strdup_printf ("Could not recover PCM device (%s)", snd_strerror (written));
				xmms_error_set (error, XMMS_ERROR_GENERIC, message);
				g_free (message);
			}
		} else {
			gchar *message = g_strdup_printf ("Unexpected error from ALSA (%s)", snd_strerror (written));
			xmms_error_set (error, XMMS_ERROR_GENERIC, message);
			g_free (message);
		}
	}
}

static snd_mixer_elem_t *
xmms_alsa_find_mixer_elem (snd_mixer_t *mixer, gint index, const char *name)
{
	snd_mixer_selem_id_t *selem_id = NULL;

	snd_mixer_selem_id_alloca (&selem_id);

	snd_mixer_selem_id_set_index (selem_id, index);
	snd_mixer_selem_id_set_name (selem_id, name);

	return snd_mixer_find_selem (mixer, selem_id);
}

static snd_mixer_selem_channel_id_t
lookup_channel (const gchar *name)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (channel_map); i++) {
		if (!strcmp (channel_map[i].name, name)) {
			return channel_map[i].id;
		}

	}

	return SND_MIXER_SCHN_UNKNOWN;
}
