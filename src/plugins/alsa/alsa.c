/** @file 
 * This file adds ALSA output.
 *
 * Written by Daniel Svensson, <nano@nittioonio.nu>
 *
 * @todo xmms2d crashes if the audiodevice is closed and then restarted. 
 * @todo mixer set/get
 * @todo get correct soundcard if default does not exist.. ie break out
 *       the opening of soundcard to a new function.
 */

#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/config_xmms.h"
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
#define SND_FRAG_SIZE    		2048
#define SND_FRAG_COUNT   		16
#define SND_STREAM       		SND_PCM_STREAM_PLAYBACK
#define SND_DEFAULT_BUFFER_TIME 500
#define SND_DEFAULT_PERIOD_TIME 50


/*
 * Type definitions
 */
typedef struct xmms_alsa_data_St {
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hwparams;
	gint frame_size;
	gint rate;
	snd_mixer_t *mixer;
	gboolean have_mixer;
	xmms_config_value_t *conf_mixer;
} xmms_alsa_data_t;


/*
 * Function prototypes
 */
static void xmms_alsa_flush (xmms_output_t *output);
static void xmms_alsa_close (xmms_output_t *output);
static void xmms_alsa_write (xmms_output_t *output, gchar *buffer, gint len);
static void xmms_alsa_xrun_recover (xmms_output_t *output);
static guint xmms_alsa_samplerate_set (xmms_output_t *output, guint rate);
static guint xmms_alsa_buffersize_get (xmms_output_t *output);
static gboolean xmms_alsa_open (xmms_output_t *output);
static gboolean xmms_alsa_new (xmms_output_t *output);
static gboolean xmms_alsa_set_hwparams (xmms_alsa_data_t *data); 
static gboolean xmms_alsa_mixer_set (xmms_output_t *output, gint left, gint right);
static gboolean xmms_alsa_mixer_get (xmms_output_t *output, gint *left, gint *right);


/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "alsa",
			"ALSA Output 0.1",
			"Advanced Linux Sound Architecture output plugin");

	xmms_plugin_info_add (plugin, "URL", "http://www.nittionio.nu/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	xmms_plugin_info_add (plugin, "E-Mail", "nano@nittionino.nu");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, xmms_alsa_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, xmms_alsa_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_alsa_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_alsa_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, xmms_alsa_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, 
			xmms_alsa_samplerate_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET,
			xmms_alsa_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_GET,
			xmms_alsa_mixer_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_MIXER_SET,
			xmms_alsa_mixer_set);

	return plugin;
}

/*
 * Member functions
 */


/**
 * Change mixer parameters.
 *
 * @param output The output structure
 */
static gboolean
xmms_alsa_mixer_set (xmms_output_t *output, gint left, gint right) {
	XMMS_DBG ("XMMS_ALSA_MIXER_SET");
	return TRUE;
}



/**
 * Get mixer parameters.
 *
 * @param output The output structure
 */
static gboolean
xmms_alsa_mixer_get (xmms_output_t *output, gint *left, gint *right) {
	XMMS_DBG ("XMMS_ALSA_MIXER_GET");
	return TRUE;
}


/**
 * Get buffersize.
 *
 * @param output The output structure.
 * @return the current buffer size or 0 on failure.
 */
static guint
xmms_alsa_buffersize_get (xmms_output_t *output) {
	xmms_alsa_data_t *data;
	snd_pcm_uframes_t *total_buf;
	snd_pcm_sframes_t avail;
	gint err;
	guint buffer_size;
	
	g_return_val_if_fail (output, 0);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, 0);

	XMMS_DBG ("XMMS_ALSA_BUFFERSIZE_GET");
	
	if ((err = snd_pcm_hw_params_get_buffer_size (data->hwparams, total_buf)) != 0) {
		XMMS_DBG ("Unable to get buffer size (%s)", snd_strerror (err));
		return FALSE;
	}

	if ((avail = snd_pcm_avail_update (data->pcm)) == 0) {
		XMMS_DBG ("Unable to get nr of frames in buffer (%s)", snd_strerror (avail));
		return FALSE;
	}

	buffer_size = (guint)(total_buf - avail) * (guint)(data->frame_size);

	return buffer_size; 
}


/**
 * Flush buffer
 *
 * @param output The output structure
 */
static void
xmms_alsa_flush (xmms_output_t *output) {
	
	xmms_alsa_data_t *data;	
	gint err;
	
	g_return_if_fail (output);
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG("XMMS_ALSA_FLUSH");
	
	if ((err = snd_pcm_reset (data->pcm)) != 0)
		XMMS_DBG ("flush failed (%s)", snd_strerror (err));

	if ((err = snd_pcm_prepare (data->pcm)) != 0)
		XMMS_DBG ("could not prepare soundcard (%s)", snd_strerror (err));
}


/** 
 * Open audio device and configure it to play a stream.
 *
 * @param output The outputstructure we are supposed to fill in with our 
 *               private alsa-blessed data
 */
static gboolean
xmms_alsa_open (xmms_output_t *output)
{
	xmms_alsa_data_t *data;
	gchar *dev;
	gint err;
	gint alsa_bits_per_sample;
	
	snd_pcm_info_t *info;
	gchar *alsa_name;
	static gint alsa_card;
	static gint alsa_device;
	static gint alsa_subdevice;


	g_return_val_if_fail (output, FALSE);
	data = xmms_output_plugin_data_get (output);
	
	XMMS_DBG("XMMS_ALSA_OPEN");	
	dev = xmms_output_config_string_get (output, "device");
	if (!dev) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "default";
	}

	XMMS_DBG ("opening device: %s", dev);
	
	if ((err = snd_pcm_open (&(data->pcm), dev, SND_STREAM, 0)) < 0) {
		xmms_log_fatal ("FATAL: %s: cannot open audio device (%s)", 
				__FILE__, snd_strerror (err));
		return FALSE;
	}



	/* extract card information */
	snd_pcm_info_malloc (&info);
	snd_pcm_info (data->pcm, info);
	
	alsa_card = snd_pcm_info_get_card (info);
	alsa_device = snd_pcm_info_get_device (info);
	alsa_subdevice = snd_pcm_info_get_subdevice (info);
	alsa_name = (gchar *)snd_pcm_info_get_name (info);
	
	snd_pcm_info_free (info);
	
	XMMS_DBG ("Card: %i, Device: %i, Subdevice: %i, Name: %s", alsa_card, 
			alsa_device, alsa_subdevice, alsa_name);
	/* end of card information */


	/* allocate hardware parameters structure */
	/*snd_pcm_hw_params_alloca (&(data->hwparams)); */

	
	alsa_bits_per_sample = snd_pcm_format_physical_width (SND_FORMAT);
	data->frame_size = (alsa_bits_per_sample * SND_CHANNELS) / 8;

	XMMS_DBG ("bps: %d, fs: %d", alsa_bits_per_sample, data->frame_size);
	
	return TRUE;
}


/***
 * New mekkodon
 *
 * @param output The output structure
 */
static gboolean
xmms_alsa_new (xmms_output_t *output) {
	xmms_alsa_data_t *data;
	
	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_alsa_data_t, 1);

	data->have_mixer = FALSE;
	XMMS_DBG ("XMMS_ALSA_NEW"); 
	
	xmms_output_plugin_data_set (output, data); 
	
	return TRUE; 
}


/**
 * Setup hardware parameters.
 *
 * @param data Internal plugin structure
 */
static gboolean 
xmms_alsa_set_hwparams(xmms_alsa_data_t *data) {
	gint err;
	gint dir;
	gint requested_buffer_time;
	gint requested_period_time;

	g_return_val_if_fail (data, 0);

	XMMS_DBG ("XMMS_ALSA_SET_HWPARAMS");
	
	/* choose all parameters */
	if ((err = snd_pcm_hw_params_any (data->pcm, data->hwparams)) < 0) {
		xmms_log_fatal ("cannot initialize hardware parameter structure (%s)", 
				snd_strerror (err));
		return FALSE;
	}

	/* set the interleaved read/write format */
	if ((err = snd_pcm_hw_params_set_access (data->pcm, 
					data->hwparams, 
					SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		xmms_log_fatal ("cannot set access type (%s)", snd_strerror (err));
		return FALSE;
	}

	/* set the sample format */
	if ((err = snd_pcm_hw_params_set_format (data->pcm, data->hwparams, 
					SND_FORMAT)) < 0) {
		xmms_log_fatal ("cannot set sample format (%s)", snd_strerror (err));
		return FALSE;
	}

	/* set the count of channels */
	if ((err = snd_pcm_hw_params_set_channels (data->pcm, data->hwparams, 
					SND_CHANNELS)) < 0) {
		xmms_log_fatal ("cannot set channel count (%s)", snd_strerror (err));
		return FALSE;
	}
	
	/* set the sample rate */
	if ((err = snd_pcm_hw_params_set_rate_near (data->pcm, data->hwparams, 
					&data->rate, 0)) < 0) {
		xmms_log_fatal ("cannot set sample rate (%s)\n", snd_strerror (err));
		return FALSE;
	}

	/* extract the rate we got */
	snd_pcm_hw_params_get_rate (data->hwparams, &err, 0);
	XMMS_DBG ("rate: %d", err);
	
	requested_buffer_time = 500000; 
	requested_period_time = 100000;
	
	if ((err = snd_pcm_hw_params_set_buffer_time_near (data->pcm, 
					data->hwparams, &requested_buffer_time, &dir)) < 0) {
		xmms_log_fatal ("Buffer time <= 0");  
		return FALSE;
	}

	XMMS_DBG ("Buffer time requested: 500ms, got: %dms", 
			requested_buffer_time / 1000); 
	
	if ((err = snd_pcm_hw_params_set_period_time_near (data->pcm, 
					data->hwparams, &requested_period_time, &dir)) < 0) {
		xmms_log_fatal ("cannot set periods (%s)", snd_strerror (err));
		return FALSE;
	}

	XMMS_DBG ("Period time requested: 100ms, got: %dms", 
			requested_period_time / 1000); 
	
	if ((err = snd_pcm_hw_params (data->pcm, data->hwparams)) < 0) {
		xmms_log_fatal ("cannot set hw parameters (%s), %d", 
				snd_strerror (err), snd_pcm_state (data->pcm));

		return FALSE;
	}

	if ((err = snd_pcm_prepare (data->pcm)) < 0) {
		xmms_log_fatal ("cannot prepare audio interface for use (%s)", 
				snd_strerror (err));
		return FALSE;
	}

	return TRUE;
}





/**
 * Set sample rate.
 *
 * @param output The output structure.
 * @param rate The to-be-set sample rate.
 * @return a guint with the sample rate.
 *
 */
static guint
xmms_alsa_samplerate_set (xmms_output_t *output, guint rate)
{
	xmms_alsa_data_t *data;
	gint err;
	
	XMMS_DBG ("XMMS_ALSA_SAMPLERATE_SET: trying to change rate to: %d", rate);
	
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
		xmms_alsa_set_hwparams (data);
	}

	return rate;
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
	
	/* No more of that disco shit! */
	if ((err = snd_pcm_drain (data->pcm)) != 0) {
		xmms_log_fatal ("error draining buffer (%s)", snd_strerror (err));
	}

	if ((err = snd_pcm_hw_free (data->pcm)) != 0) {
		xmms_log_fatal ("error freeing hwparams (%s)", snd_strerror (err));
	}
	
	snd_pcm_hw_params_free (data->hwparams);
	
	/* Close device */
	if ((err = snd_pcm_close (data->pcm)) != 0) {
		xmms_log_fatal ("error closing audio device (%s)", snd_strerror (err));
	} else {
		XMMS_DBG ("audio device closed");
	}
	
}


/**
 * XRUN recovery.
 * @param output The output structure filled with alsa data. 
 */
static void
xmms_alsa_xrun_recover (xmms_output_t *output)
{
	gint err;
	xmms_alsa_data_t *data;
	
	g_return_if_fail (output);
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG ("xrun recover");
	
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

	g_return_if_fail (buffer);
	g_return_if_fail (len);
	g_return_if_fail (output);
	
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
			xmms_alsa_xrun_recover (output);
		}
		else {
			xmms_log_debug ("read/write error: %d", written_frames);
		}
	}
}
