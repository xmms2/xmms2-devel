/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#define BUILD_TARGET_ARM 1
#include "cooler-core.h"
#include "cooler-media.h"

#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#define DEFAULT_SAMPLERATE 44100
#define DEFAULT_CHANNELS 2
#define SUPPORTED_CHANNELS 1
#define SUPPORTED_SAMPLERATES 12

#define NEUROS_OUTPUT_PLUGIN_PATH NMS_PLUGIN_DIR "libdm320nmso.so"
#define NO_BUFFERS_SLEEP_TIME (10 * 1000) //10 ms
#define FAUX_BUFFER_SIZE (1024 * 128)

//our supported
static int samplerate[] = {
8000,
11025,
12000,
16000,
22050,
24000,
32000,
44100,
48000,
64000,
88200,
96000,
};
static int channels[] ={
//1, DM320 PCM out doesn't really support mono, so we let the xmms2 converter kick in
2,
};

static media_desc_t desc = {
	.adesc = {
		.audio_type = NMS_AC_PCM,
		.num_channels = DEFAULT_CHANNELS,
		.sample_rate = DEFAULT_SAMPLERATE,
		.bitrate = DEFAULT_CHANNELS * DEFAULT_SAMPLERATE * 16,
	},
	.vdesc = {
		.video_type = NMS_VC_NO_VIDEO,
		.width = 0,
		.height = 0,
		.frame_rate = 0,
	},
	.sdesc = {
		.subtitle_type = NMS_SC_NO_SUBTITLE,
	},
};


/*
 * Type definitions
 */

typedef struct xmms_nms_data_t {
	media_output_plugin_t *neuros_plugin;
	int neuros_plugin_error;
	media_buf_t buf;
} xmms_nms_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_nms_plugin_setup (xmms_output_plugin_t *output_plugin);
static gboolean xmms_nms_new (xmms_output_t *output);
static void xmms_nms_destroy (xmms_output_t *output);
static void xmms_nms_flush (xmms_output_t *output);
static gboolean xmms_nms_open (xmms_output_t *output);
static void xmms_nms_close (xmms_output_t *output);
static void xmms_nms_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err);
static gboolean xmms_nms_format_set (xmms_output_t *output, const xmms_stream_type_t *format);
static guint xmms_nms_latency_get (xmms_output_t *output);

/*
 * Plugin header
 */
XMMS_OUTPUT_PLUGIN ("nms",
                    "NMS output",
                    XMMS_VERSION,
                    "Neuros output plugin",
                    xmms_nms_plugin_setup);

static gboolean
xmms_nms_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);
	methods.new = xmms_nms_new;
	methods.destroy = xmms_nms_destroy;
	methods.flush = xmms_nms_flush;

	methods.open = xmms_nms_open;
	methods.close = xmms_nms_close;
	methods.format_set = xmms_nms_format_set;
	methods.write = xmms_nms_write;
	methods.latency_get = xmms_nms_latency_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	return TRUE;
}

static gboolean
xmms_nms_new (xmms_output_t *output)
{
	xmms_nms_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_nms_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	int i, j;
	for (i = 0; i < SUPPORTED_CHANNELS; i++) {
		for (j = 0; j < SUPPORTED_SAMPLERATES; j++) {
			xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_S16, channels[i], samplerate[j]);
		}
	}
	xmms_output_private_data_set (output, data);

	return TRUE;
}

static void
xmms_nms_destroy (xmms_output_t *output)
{
	xmms_nms_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	g_free (data);
}

static void
xmms_nms_flush (xmms_output_t *output)
{
	xmms_nms_data_t *data;

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	xmms_log_info ("Neuros Output Plugin calling flush\n");

	if (data->neuros_plugin)
		data->neuros_plugin->flush (1);
}

static gboolean
xmms_nms_open (xmms_output_t *output)
{
	xmms_nms_data_t *data;
	void *plugin_lib;
	GetNmsOutputPluginFunc interface_get;
	int ret;

	data = xmms_output_private_data_get (output);
	if (data == NULL) {
		xmms_log_error ("Eeek! xmms_output_private_data_get returned NULL ?\n");
		return FALSE;
	}

	plugin_lib = dlopen (NEUROS_OUTPUT_PLUGIN_PATH, RTLD_LAZY);
	if (plugin_lib == NULL) {
		xmms_log_error ("Neuros Output Plugin is not in %s\n", NEUROS_OUTPUT_PLUGIN_PATH);
		return FALSE;
	}

	interface_get = dlsym (plugin_lib, NMS_PLUGIN_SYMBOL_OUTPUT);
	if (interface_get == NULL) {
		xmms_log_error ("Neuros Output Plugin has no interface symbol %s\n", NMS_PLUGIN_SYMBOL_OUTPUT);
		return FALSE;
	}

	data->neuros_plugin = interface_get ();
	if (data->neuros_plugin == NULL) {
		xmms_log_error ("Neuros Output Plugin interface not found\n");
		return FALSE;
	}

	ret = data->neuros_plugin->init (&desc, 0, 0);
	if (ret != 0) {
		data->neuros_plugin = NULL;
		data->neuros_plugin_error = ret;
		if (ret == 1) {
			xmms_log_error ("Neuros output plugin did not init: dm320 output locked.");
			return TRUE;
		} else {
			xmms_log_error ("Neuros output plugin failed init: error %d.", ret);
			return TRUE;
		}
	}

	ret = data->neuros_plugin->start ();
	if (ret != 0) {
		data->neuros_plugin = NULL;
		data->neuros_plugin_error = ret;
		xmms_log_error ("Error starting neuros output plugin. error: %d", ret);
		return TRUE;
	}

	return TRUE;
}

static void
xmms_nms_close (xmms_output_t *output)
{
	xmms_nms_data_t *data;

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data && data->neuros_plugin);

	xmms_log_info ("Neuros Output Plugin calling finish\n");

	//Wait for all the data in buffer to drain to speakers.
	//In case this is a STOP command, flush is already called.
	data->neuros_plugin->finish (1);
}

static void
xmms_nms_write (xmms_output_t *output, gpointer buffer, gint len, xmms_error_t *err)
{
	xmms_nms_data_t *data;
	int ret;

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->neuros_plugin == NULL) {
		//this is the result of a previous error during init, let's report it now that we can.
		if (data->neuros_plugin_error == 1) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "Neuros Output Plugin failed to init: dm320 output locked.");
		} else {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "Neuros Output Plugin failed to init or start.");
		}
		return;
	}

	/* HACK:
	We keep checking the latency of the dm320 buffer until it's below a certain threshold, then we add data to it.
	This effectively simulate a buffer small enough to prevent any humanly noticeable latency problems.
	This hack is needed because dm320 system has huge output buffers, up to 8 seconds of audio or more at times.
	Since decoding is generally much faster than playback, we have a huge latency. Xmms2 is not yet capable of
	handling hight latency in the appropriate way (calling latency_get to correct playcount skew is just a small
	part of what is needed to fully support this). Fixing this is a big job inside the xmms2 guts, appearently.
	And actually lowering the size of the dm320 buffers seems to be a pretty daunting prospect, according to Gao.
	So we resort to this hack. Let's live with it for the time being.
	*/
	while (data->neuros_plugin->bufferedData (NMS_BUFFER_AUDIO) >= FAUX_BUFFER_SIZE) {
		usleep (NO_BUFFERS_SLEEP_TIME);
	}

	// If no REAL dm320 buffer available at the moment, keep trying until there's a free one.
	// if the faux hack buffer is enabled, a real buffer will be surely available, unless some failure happens
	ret = data->neuros_plugin->getBuffer (&data->buf, 0, 0);
	while (ret == 1) {
		usleep (NO_BUFFERS_SLEEP_TIME);
	}
	if (ret < 0) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Neuros Output Plugin failed to get output buffer!");
		xmms_log_error ("Neuros Output Plugin failed to get output buffer!");
		return;
	}

	// If this happens, it means imem has some problems allocating buffers. No hope of recovery, in my opinion, so we bail
	if (data->buf.abuf.size == 0) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Neuros Output Plugin returned buffer with size zero!");
		xmms_log_error ("Neuros Output Plugin returned buffer with size zero!");
		return;
	} else {
		int sz, written;

		sz = (data->buf.abuf.size < len) ? data->buf.abuf.size : len;
		written = 0;
		while (written < len) {
			memcpy (data->buf.abuf.data, buffer + written, sz);
			data->buf.abuf.size = sz;
			data->buf.curbuf = &data->buf.abuf;
			data->neuros_plugin->write (&data->buf);
			written += sz;
		}
	}
}

static gboolean
xmms_nms_format_set (xmms_output_t *output, const xmms_stream_type_t *format)
{
	gint sformat, schannels, srate;
	xmms_nms_data_t *data;
	int ret;

	data = xmms_output_private_data_get (output);
	if (data == NULL)
		return FALSE;

	sformat = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_FORMAT);
	schannels = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_CHANNELS);
	srate = xmms_stream_type_get_int (format, XMMS_STREAM_TYPE_FMT_SAMPLERATE);

	if (data->neuros_plugin) {
		data->neuros_plugin->finish (0);

		desc.adesc.num_channels = schannels;
		desc.adesc.sample_rate = srate;
		desc.adesc.bitrate = schannels * srate * 16;

		xmms_log_info ("Neuros Output Plugin calling new format set (rate:%d format:%d channels:%d)\n",srate,sformat,schannels);

		ret = data->neuros_plugin->init (&desc, 0, 0);
		if (ret != 0) {
			data->neuros_plugin = NULL;
			data->neuros_plugin_error = ret;
			if (ret == 1) {
				xmms_log_error ("Neuros output plugin did not init: dm320 output locked.");
				return FALSE;
			} else {
				xmms_log_error ("Neuros output plugin failed init: error %d.", ret);
				return TRUE;
			}
		}

		data->neuros_plugin->start ();
	}

	return TRUE;
}

static guint
xmms_nms_latency_get (xmms_output_t *output)
{
	xmms_nms_data_t *data;
	unsigned int latency;

	data = xmms_output_private_data_get (output);
	if (data == NULL) return 0;

	if (data->neuros_plugin) {
		latency = data->neuros_plugin->bufferedData (NMS_BUFFER_AUDIO);
	} else {
		latency = 0;
	}

	return (guint) latency; //there may be a loss here, as we return the latency as a unsigned long
}
