/** @file 
 * This file adds ALSA output.
 *
 * @todo Fiddle around with the buffer so that no xruns wil appear.
 */
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/plugin.h"

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

/*
 *  Defines
 */
#define SND_RATE         44100
#define SND_CHANNELS     2
#define SND_FORMAT       SND_PCM_FORMAT_S16_LE
#define SND_PERIODS      2
#define SND_BUFF_SIZE    8192
#define SND_FRAG_SIZE    2048
#define SND_FRAG_COUNT   16
#define SND_STREAM       SND_PCM_STREAM_PLAYBACK


/*
 * Type definitions
 */

typedef struct xmms_alsa_data_St {
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hwparams;
	gint alsa_frame_size;
	gint written_frames;
} xmms_alsa_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_alsa_open (xmms_output_t *output);
static guint xmms_alsa_samplerate_set (xmms_output_t *output, guint rate);
static void xmms_alsa_close (xmms_output_t *output);
static void xmms_alsa_write (xmms_output_t *output, gchar *buffer, gint len);
static void xmms_alsa_xrun_recover (xmms_output_t *output);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "alsa",
			"ALSA Output " VERSION,
			"Advanced Linux Sound Architecture output plugin");

	xmms_plugin_info_add (plugin, "URL", "http://www.nittionio.nu/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	xmms_plugin_info_add (plugin, "E-Mail", "nano@nittionino.nu");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, xmms_alsa_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, xmms_alsa_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_alsa_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, xmms_alsa_samplerate_set);

	return plugin;
}

/*
 * Member functions
 */



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
	
	data = g_new0 (xmms_alsa_data_t, 1);
		
	dev = xmms_output_config_string_get (output, "device");
	if (!dev) {
		XMMS_DBG ("Device not found in config, using default");
		dev = "default";
	}

	XMMS_DBG ("opening device: %s", dev);
	
	if ((err = snd_pcm_open (&(data->pcm), "default", SND_STREAM, 0)) < 0) {
		xmms_log_fatal ("cannot open audio device default (%s)", 
				snd_strerror (err));
		goto error;
	}
	
	snd_pcm_info_malloc (&info);
	snd_pcm_info (data->pcm, info);
	
	alsa_card = snd_pcm_info_get_card (info);
	alsa_device = snd_pcm_info_get_device (info);
	alsa_subdevice = snd_pcm_info_get_subdevice (info);
	alsa_name = (gchar *)snd_pcm_info_get_name (info);
	
	snd_pcm_info_free(info);
	
	XMMS_DBG ("Card: %i, Device: %i, Subdevice: %i, Name: %s", alsa_card, 
			alsa_device, alsa_subdevice, alsa_name);
	
	snd_pcm_hw_params_alloca (&(data->hwparams));
	
	if ((err = snd_pcm_hw_params_any (data->pcm, data->hwparams)) < 0) {
		xmms_log_fatal ("cannot initialize hardware parameter structure (%s)", 
				snd_strerror (err));
		goto error;
	}
	
	if ((err = snd_pcm_hw_params_set_access (data->pcm, 
					data->hwparams, 
					SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		xmms_log_fatal ("cannot set access type (%s)", snd_strerror (err));
		goto error;
	}
	
	if ((err = snd_pcm_hw_params_set_format (data->pcm, data->hwparams, 
					SND_FORMAT)) < 0) {
		xmms_log_fatal ("cannot set sample format (%s)", snd_strerror (err));
		goto error;
	}
	
	if ((err = snd_pcm_hw_params_set_rate_near (data->pcm, data->hwparams, 
					SND_RATE, 0)) < 0) {
		xmms_log_fatal ("cannot set sample rate (%s)\n", snd_strerror (err));
		goto error;
	}
	
	if ((err = snd_pcm_hw_params_set_channels (data->pcm, data->hwparams, 
					SND_CHANNELS)) < 0) {
		xmms_log_fatal ("cannot set channel count (%s)", snd_strerror (err));
		goto error;
	}
	
	if ((err = snd_pcm_hw_params_set_periods (data->pcm, data->hwparams, 
					SND_PERIODS, 0)) < 0) {
		xmms_log_fatal ("cannot set periods (%s)", snd_strerror (err));
		goto error;
	}
	
	if ((err = snd_pcm_hw_params_set_buffer_size_near (data->pcm, data->hwparams, 
					(SND_BUFF_SIZE * SND_PERIODS)>>2)) < 0) {
		xmms_log_fatal ("cannot set buffer size (%s)", snd_strerror (err));
		goto error;
	}
	
	if ((err = snd_pcm_hw_params (data->pcm, data->hwparams)) < 0) {
		xmms_log_fatal ("cannot set hw parameters (%s)", snd_strerror (err));
		goto error;
	}
	
	snd_pcm_hw_params_free (data->hwparams);

	if ((err = snd_pcm_prepare (data->pcm)) < 0) {
		xmms_log_fatal ("cannot prepare audio interface for use (%s)", 
				snd_strerror (err));
		goto error;
	}
	
	alsa_bits_per_sample = snd_pcm_format_physical_width (SND_FORMAT);
	data->alsa_frame_size = (alsa_bits_per_sample * SND_CHANNELS) / 8;                                                
	XMMS_DBG ("bps: %d, fs: %d", alsa_bits_per_sample, data->alsa_frame_size);
	
	xmms_output_plugin_data_set (output, data);

	return TRUE;
error:
	xmms_alsa_close ((xmms_output_t *)data);
	g_free (data);
	return FALSE;
}


/**
 * Set sample rate.
 *
 * @param output The output structure.
 * @param rate The to-be-set sample rate.
 * @return a guint with the sample rate.
 */
static guint
xmms_alsa_samplerate_set (xmms_output_t *output, guint rate)
{
	return 44100;
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

	if ((err = snd_pcm_close (data->pcm)) != 0)
		xmms_log_fatal ("error closing audio device (%s)", snd_strerror (err));
	else
		XMMS_DBG ("audio device closed");

	g_free (data);
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
	
	if (snd_pcm_state (data->pcm) == SND_PCM_STATE_XRUN)
		if ((err = snd_pcm_prepare (data->pcm)) < 0)
			xmms_log_fatal ("xrun: prepare error, %s", snd_strerror (err));
}



/**
 * Write data to the output device.
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

	g_return_if_fail (buffer);
	g_return_if_fail (len);
	g_return_if_fail (output);
	
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	while (len > 0) {

		if ((data->written_frames = snd_pcm_writei (data->pcm, buffer, 
						len / data->alsa_frame_size)) > 0) {
			written = data->written_frames * data->alsa_frame_size;
			len -= written;
			buffer += written;
		}
		else if (data->written_frames == -EAGAIN || (data->written_frames > 0 && 
					data->written_frames < (len / data->alsa_frame_size))) {
			snd_pcm_wait (data->pcm, 100);
		}
		else if (data->written_frames == -EPIPE) {
			xmms_alsa_xrun_recover (output);
		}
		else {
			xmms_log_fatal ("read/write error: %d", data->written_frames);
			break;
		}
	}
}
