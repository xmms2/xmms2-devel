/** @file 
 * This file adds ALSA output.
 *
 * Written by Daniel Svensson, <nano@nittioonio.nu>
 *
 * @todo mixer set/get (low priority.. a first stable version is prime directive)
 * @todo get correct soundcard if default does not exist.. probably break out
 *       opening of soundcard to a new function or something.
 * @todo unable to retrive buffer size. to reproduce do lots of stuff fast.
 *       (play, stop, next, prev) .. to fix.. stop and play again..
 *       determine a way to fix this problem. Perhaps something with snd_pcm_state?
 *       (snd_strerror gives: Invalid argument). Do some google work.
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
#define SND_STREAM       		SND_PCM_STREAM_PLAYBACK


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
			"ALSA Output" XMMS_VERSION,
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


	return (plugin);
}

/*
 * Member functions
 */


/**
 * Change mixer parameters.
 *
 * @param output The output structure
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_alsa_mixer_set (xmms_output_t *output, gint left, gint right) 
{
	XMMS_DBG ("XMMS_ALSA_MIXER_SET");
	
	g_return_val_if_fail (output, FALSE);
	
	
	return FALSE;
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
	XMMS_DBG ("XMMS_ALSA_MIXER_GET");

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (right, FALSE);
	g_return_val_if_fail (left, FALSE);
	
	return FALSE;
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
	snd_pcm_uframes_t *total_buf;
	snd_pcm_sframes_t avail;
	gint err;
	guint buffer_size;
	
/*	XMMS_DBG ("XMMS_ALSA_BUFFERSIZE_GET"); */
	
	g_return_val_if_fail (output, 0);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, 0);

	/* Get size of buffer in frames */
	if ((err = snd_pcm_hw_params_get_buffer_size (data->hwparams, total_buf)) != 0) {
		XMMS_DBG ("Unable to get buffer size (%s)", snd_strerror (err));
		return FALSE;
	}

	/* Get number of available frames in buffer */
	if ((avail = snd_pcm_avail_update (data->pcm)) == 0) {
		XMMS_DBG ("Unable to get number of frames in buffer (%s)", 
				snd_strerror (avail));
		return FALSE;
	}

	buffer_size = (guint)(total_buf - avail) * (guint)(data->frame_size);

/*	XMMS_DBG ("Buffer size: %d", buffer_size); */
	
	return buffer_size; 
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
	
	if ((err = snd_pcm_reset (data->pcm)) != 0)
		XMMS_DBG ("Flush failed (%s)", snd_strerror (err));

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
	gchar *dev;
	gint err;
	gint alsa_bits_per_sample;
	
	XMMS_DBG ("XMMS_ALSA_OPEN");	

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_plugin_data_get (output);
	g_return_val_if_fail (data, FALSE);
	
	dev = xmms_output_config_string_get (output, "device");
	if (!dev) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "default";
	}

	XMMS_DBG ("Opening device: %s", dev);

	/* Open the device */
	if ((err = snd_pcm_open (&(data->pcm), dev, SND_STREAM, 0)) < 0) {
		xmms_log_fatal ("FATAL: %s: cannot open audio device (%s)", 
				__FILE__, snd_strerror (err));
		return FALSE;
	}

	alsa_bits_per_sample = snd_pcm_format_physical_width (SND_FORMAT);
	data->frame_size = (gint)(alsa_bits_per_sample * SND_CHANNELS) / 8;

	XMMS_DBG ("bps: %d, frame size: %d", alsa_bits_per_sample, data->frame_size);
	
	return TRUE;
}


/***
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

	data->have_mixer = FALSE;
	
	xmms_output_plugin_data_set (output, data); 
	
	return TRUE; 
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
		xmms_log_fatal ("Buffer time <= 0 (%s)", snd_strerror (err));  
		return FALSE;
	}

	XMMS_DBG ("Buffer time requested: 500ms, got: %dms", 
			requested_buffer_time / 1000); 

	/* Set period time */
	if ((err = snd_pcm_hw_params_set_period_time_near (data->pcm, 
					data->hwparams, &requested_period_time, &dir)) < 0) {
		xmms_log_fatal ("cannot set periods (%s)", snd_strerror (err));
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
	
	/* Close device */
	if ((err = snd_pcm_close (data->pcm)) != 0) 
		xmms_log_fatal ("Audio device could not be released. (%s)", 
				snd_strerror (err));
	else 
		XMMS_DBG ("audio device closed");

	/* reset rate */
	data->rate = 0;
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
