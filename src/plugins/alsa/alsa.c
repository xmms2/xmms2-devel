/** @file 
 * This file adds ALSA output.
 */
#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/ringbuf.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>


#include <glib.h>


#include <alsa/asoundlib.h>
#include <alsa/pcm.h>


/*
 * Type definitions
 */

typedef struct xmms_alsa_data_St {
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	gint alsa_frame_size;
} xmms_alsa_data_t;

typedef struct _snd_format {
	guint rate;
	guint channels;
	snd_pcm_format_t format;
	guint periods;
	guint buffer_size;
} snd_format_t;


static gint written_frames     = 0;
static gint alsa_total_written = 0;



/*
 * Function prototypes
 */

static gboolean xmms_alsa_open (xmms_output_t *output);
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

	xmms_plugin_info_add (plugin, "URL", "http://www.alsa-project.org/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, xmms_alsa_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, xmms_alsa_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_alsa_close);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_alsa_open (xmms_output_t *output)
{
	xmms_alsa_data_t *data;
	snd_format_t *fmt;
	
	gchar *dev;
	gint err;
	
	gint alsa_bits_per_sample;

#ifdef DEBUG
	snd_pcm_info_t *info;
	static gint alsa_card      = 0;
	static gint alsa_device    = 0;
	static gint alsa_subdevice = 0;
#endif
	

	
	data = g_new0 (xmms_alsa_data_t, 1);
	fmt = g_new0 (snd_format_t, 1);
	
	fmt->rate        = 44100;
	fmt->channels    = 2;
	fmt->format      = SND_PCM_FORMAT_S16_LE;
	fmt->periods     = 2;
	fmt->buffer_size = 8192;

	
	dev = xmms_output_get_config_string (output, "device");

	if (!dev) {
		XMMS_DBG("Device not found in config, using default");
		dev = "default";
	}

	XMMS_DBG("opening device: %s", dev);
	
	if ((err = snd_pcm_open (&(data->pcm), "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		XMMS_DBG ("cannot open audio device default (%s)", snd_strerror (err));
		return FALSE;
	}

	
#ifdef DEBUG
	snd_pcm_info_malloc(&info);
	snd_pcm_info(data->pcm, info);
	alsa_card      = snd_pcm_info_get_card(info);
	alsa_device    = snd_pcm_info_get_device(info);
	alsa_subdevice = snd_pcm_info_get_subdevice(info);
	XMMS_DBG ("Card: %i, Device: %i, Subdevice: %i", alsa_card, alsa_device, alsa_subdevice); 
#endif

	
	snd_pcm_hw_params_alloca(&(data->hwparams));
	snd_pcm_sw_params_alloca(&(data->swparams));
	
	
	if ((err = snd_pcm_hw_params_any(data->pcm, data->hwparams)) < 0) {
		XMMS_DBG("cannot initialize hardware parameter structure (%s)", snd_strerror (err));
		return FALSE;
	}
	if ((err = snd_pcm_hw_params_set_access (data->pcm, data->hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		XMMS_DBG ("cannot set access type (%s)", snd_strerror (err));
		return FALSE;
	}
	if ((err = snd_pcm_hw_params_set_format (data->pcm, data->hwparams, fmt->format)) < 0) {
		XMMS_DBG ("cannot set sample format (%s)", snd_strerror (err));
		return FALSE;
	}
	if ((err = snd_pcm_hw_params_set_rate_near (data->pcm, data->hwparams, fmt->rate, 0)) < 0) {
		XMMS_DBG ("cannot set sample rate (%s)\n", 	snd_strerror (err));
		return FALSE;
	}
	if ((err = snd_pcm_hw_params_set_channels (data->pcm, data->hwparams, fmt->channels)) < 0) {
		XMMS_DBG ("cannot set channel count (%s)", snd_strerror (err));
		return FALSE;
	}
	if ((err = snd_pcm_hw_params_set_periods(data->pcm, data->hwparams, fmt->periods, 0)) < 0) {
		XMMS_DBG ("cannot set periods (%s)", snd_strerror (err));
		return FALSE;
	}
	if ((err = snd_pcm_hw_params_set_buffer_size_near(data->pcm, data->hwparams, (fmt->buffer_size * fmt->periods)>>2)) < 0) {
		XMMS_DBG ("cannot set buffer size (%s)", snd_strerror (err));
	}
	

	if ((err = snd_pcm_hw_params (data->pcm, data->hwparams)) < 0) {
		XMMS_DBG ("cannot set hw parameters (%s)", snd_strerror (err));
		return FALSE;
	}
/*	if ((err = snd_pcm_sw_params (data->pcm, data->swparams)) < 0) {
		XMMS_DBG ("cannot set sw parameters (%s)", snd_strerror (err));
		return FALSE;
	} */
	snd_pcm_hw_params_free (data->hwparams);
	snd_pcm_sw_params_free (data->swparams);


	
	if ((err = snd_pcm_prepare (data->pcm)) < 0) {
		XMMS_DBG ("cannot prepare audio interface for use (%s)", snd_strerror (err));
		return FALSE;
	}

	alsa_bits_per_sample = snd_pcm_format_physical_width(fmt->format);
	data->alsa_frame_size = (alsa_bits_per_sample * fmt->channels) / 8;                                                
	XMMS_DBG ("bps: %d, fs: %d", alsa_bits_per_sample, data->alsa_frame_size);
	/**	alsa_bps = (alsa_rate * data->alsa_frame_size) / 1000;                                                              */
				
	
	xmms_output_plugin_data_set(output, data);

	return TRUE;
}

static void
xmms_alsa_close (xmms_output_t *output)
{
	gint err;
	xmms_alsa_data_t *data;


	g_return_if_fail (output);
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	if ((err = snd_pcm_close(data->pcm)) != 0) {
		XMMS_DBG("error closing audio device");
	} else {
		XMMS_DBG("audio device closed");
	}
}

static void
xmms_alsa_xrun_recover (xmms_output_t *output)
{
	gint err;
	xmms_alsa_data_t *data;
	
	g_return_if_fail (output);

	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG("xrun recover");
	
	if (snd_pcm_state(data->pcm) == SND_PCM_STATE_XRUN) {
		if ((err = snd_pcm_prepare(data->pcm)) < 0) {
			XMMS_DBG("xrun: prepare error, %d", err);                                                              
		}                                                                                                        
	}     
}


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
				
		written_frames = snd_pcm_writei(data->pcm, buffer, len / data->alsa_frame_size);

		if (written_frames > 0) {
			written = written_frames * data->alsa_frame_size;
			alsa_total_written += written;
			len -= written;
			buffer += written;
		}
		else if (written_frames == -EAGAIN || (written_frames > 0 && written_frames < (len / data->alsa_frame_size))) {
			snd_pcm_wait(data->pcm, 100);
		}
		else if (written_frames == -EPIPE) {
			xmms_alsa_xrun_recover(output);
		}
		else {
			XMMS_DBG("read/write error: %d", written_frames);
			break;
		}
	}
}
