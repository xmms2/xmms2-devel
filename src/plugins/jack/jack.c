/*  XMMS2 - Jack Output Plugin
 *  Copyright (C) 2004-2006 Chris Morgan <cmorgan@alum.wpi.edu>
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

#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>
#include <stdlib.h>
#include <unistd.h>
#include <jack/jack.h>
#include <sys/time.h>



/*
 *  Defines
 */
#define ERR_SUCCESS                           0
#define ERR_OPENING_JACK                      1
#define ERR_RATE_MISMATCH                     2
#define ERR_BYTES_PER_OUTPUT_FRAME_INVALID    3
#define ERR_BYTES_PER_INPUT_FRAME_INVALID     4
#define ERR_TOO_MANY_OUTPUT_CHANNELS          5
#define ERR_PORT_NAME_OUTPUT_CHANNEL_MISMATCH 6
#define ERR_PORT_NOT_FOUND                    7


#define CALLBACK_TRACE     0

#if CALLBACK_TRACE
#define XMMS_CALLBACK_DBG      XMMS_DBG
#else
#define XMMS_CALLBACK_DBG(...) do {} while(0)
#endif



/*
 * Type definitions
 */
enum status_enum {
	PLAYING,
	PAUSED,
	STOPPED,
	CLOSED,
	RESET
};

typedef struct xmms_jack_data_St {
	guint rate;
	gboolean have_mixer;
	xmms_config_property_t *mixer_conf;

	/* temporary buffer used to process
	   data before sending to jack */
	guchar *sound_buffer;

	/* output ports */
	jack_port_t **output_port;

	/* pointer to jack client */
	jack_client_t *client;

	/* one of PLAYING, PAUSED, STOPPED,
	 * CLOSED, RESET etc */
	enum status_enum state;

	/* array of strings that represents
	   channel names, null terminated */
	gchar **channel_names;

	gfloat *volume;

	/* samples (frames) per second */
	glong sample_rate;

	/* number of input channels
	   1 is mono, 2 stereo etc */
	gulong num_input_channels;

	/* number of output channels
	   1 is mono, 2 stereo etc */
	gulong num_output_channels;

	/* number of bytes in the buffer
	   allocated for processing data
	   in JACK_Callback */
	gulong buffer_size;

} xmms_jack_data_t;



/*
 * Function prototypes
 */
static gboolean xmms_jack_new (xmms_output_t *output);
static void xmms_jack_destroy (xmms_output_t *output);
static void xmms_jack_flush (xmms_output_t *output);
static gboolean xmms_jack_volume_set (xmms_output_t *output,
                                      const gchar *channel,
                                      guint volume);
static gboolean xmms_jack_volume_get (xmms_output_t *output,
                                      const gchar **names, guint *values,
                                      guint *num_channels);
static guint xmms_jack_buffersize_get (xmms_output_t *output);
static gboolean xmms_jack_status (xmms_output_t *output,
                                  xmms_playback_status_t status);
static gint xmms_jack_open_device (xmms_output_t *output);
static gint xmms_jack_open (xmms_output_t *output, guint bytes_per_channel,
                            gulong *rate, gint channels);
static gboolean xmms_jack_start (xmms_output_t *output);
static gboolean xmms_jack_plugin_setup (xmms_output_plugin_t *plugin);
static void xmms_jack_float_volume_effect (jack_default_audio_sample_t **buff,
                                           gulong nsamples,
                                           guint output_channels,
                                           gfloat *volume);
static void xmms_jack_sample_silence_ds (jack_default_audio_sample_t *dst,
                                         gulong nsamples);
static int xmms_jack_callback (jack_nframes_t nframes, void *arg);



/*
 * Plugin header
 */
XMMS_OUTPUT_PLUGIN ("jack", "Jack Output", XMMS_VERSION,
                    "Jack audio server output plugin",
                    xmms_jack_plugin_setup);

static gboolean
xmms_jack_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_jack_new;
	methods.destroy = xmms_jack_destroy;

	methods.flush = xmms_jack_flush;

	methods.volume_get = xmms_jack_volume_get;
	methods.volume_set = xmms_jack_volume_set;

	methods.status = xmms_jack_status;

	methods.latency_get = xmms_jack_buffersize_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	return TRUE;
}



/*
 * Member functions
 */

/**
 * Floating point volume routine.
 * Volume should be a value between 0.0 and 1.0.
 *
 * @param sample buffer to apply volume adjustment to
 * @param number of samples to process
 * @param value to apply to these samples
 */
static void
xmms_jack_float_volume_effect (jack_default_audio_sample_t **buffer,
                               gulong nsamples, guint output_channels,
                               gfloat *volume)
{
	jack_default_audio_sample_t *buf;
	gulong samples;
	gfloat vol;
	guint x;

	/* process each output channel */
	for (x = 0; x < output_channels; x++) {
		buf = buffer[x];
		vol = volume[x];
		samples = nsamples;

		vol = CLAMP (vol, 0.0, 1.0);

		while (samples--) {
			*buf = (*buf) * vol;
			buf++;
		}
	}
}


/**
 * Fill dst buffer with nsamples worth of silence.
 *
 * @param buffer to fill with silence
 * @param number of samples of silence to fill the buffer with
 */
static void
xmms_jack_sample_silence_ds (jack_default_audio_sample_t *dst, gulong nsamples)
{
	/* ALERT: signed sign-extension portability !!! */
	while (nsamples--) {
		*dst = 0;
		dst++;
	}
}


/**
 * Main callback, jack calls this function anytime it needs new data.
 *
 * @param nframes number of frames of data that jack wants
 * @param arg pointer to xmms output structure
 * @return always zero
 */
static int
xmms_jack_callback (jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t **out_buffer;
	xmms_output_t *output = (xmms_output_t*) arg;
	xmms_jack_data_t *data;
	gint i;

	XMMS_CALLBACK_DBG ("nframes %ld, "
	                   "sizeof(jack_default_audio_sample_t) == %ld",
	                   (long) nframes, sizeof (jack_default_audio_sample_t));

	data = xmms_output_private_data_get (output);

	if (!data->client) {
		xmms_log_fatal ("client is closed, this is weird...");
	}

	XMMS_CALLBACK_DBG ("num_output_channels = %ld, num_input_channels = %ld",
	                   data->num_output_channels, data->num_input_channels);

	/* retrieve the buffers for the output ports */
	out_buffer = g_new (jack_default_audio_sample_t *,
	                    data->num_output_channels);

	for (i = 0; i < data->num_output_channels; i++) {
		out_buffer[i] = (jack_default_audio_sample_t *)
		                 jack_port_get_buffer (data->output_port[i], nframes);
	}

	/* handle playing state */
	if (data->state == PLAYING) {
		gulong tmp;

		/* frames we have left to write to jack */
		gulong jackFramesAvailable = nframes;

		/* frames we have available this loop */
		glong inputFramesAvailable;

		/* num frames we are writing this loop */
		gulong numFramesToWrite;

		XMMS_CALLBACK_DBG ("playing... jackFramesAvailable = %ld",
		                   jackFramesAvailable);

		/* see if our buffer is large enough for the data we are writing
		 * ie. Buffer_size < (bytes we already wrote + bytes we are
		 * going to write in this loop).
		 * Note: sound_buffer is always filled with 16-bit data
		 * so frame * 2 bytes(16 bits) * X output channels */
		tmp = jackFramesAvailable * sizeof (gfloat) * data->num_output_channels;

		if (data->buffer_size < tmp) {
			xmms_log_fatal ("our buffer must have changed size, "
			                "allocated %ld bytes, need %ld bytes",
			                data->buffer_size, tmp);
			g_free (out_buffer); /* free output buffer ports */
			return 0;
		}

		tmp = jackFramesAvailable * sizeof (gfloat) * data->num_input_channels;
		XMMS_CALLBACK_DBG ("trying to read %ld bytes\n", tmp);

		inputFramesAvailable = xmms_output_read (output,
		                                         (gchar *) data->sound_buffer,
		                                         tmp);

		if (inputFramesAvailable == -1) {
			inputFramesAvailable = 0;
		}

		inputFramesAvailable = inputFramesAvailable /
		                       (sizeof (gfloat) * data->num_input_channels);

		XMMS_CALLBACK_DBG ("inputFramesAvailable == %ld, "
		                   "jackFramesAvailable == %ld",
		                   inputFramesAvailable, jackFramesAvailable);

		/* write as many bytes as we have space remaining,
		 * or as much as we have data to write */
		numFramesToWrite = MIN (jackFramesAvailable, inputFramesAvailable);

		XMMS_CALLBACK_DBG ("nframes == %ld, jackFramesAvailable == %ld,\n"
		                   "\tdata->num_input_channels == %ld,"
		                   "data->num_output_channels == %ld",
		                   (glong) nframes, jackFramesAvailable,
		                   data->num_input_channels,
		                   data->num_output_channels);

		for (i = 0; i < numFramesToWrite; i++) {
			out_buffer[0][i] = ((gfloat*) data->sound_buffer)[i*2];
			out_buffer[1][i] = ((gfloat*) data->sound_buffer)[i*2+1];
		}

		/* take away what was written */
		jackFramesAvailable -= numFramesToWrite;

		XMMS_CALLBACK_DBG ("jackFramesAvailable == %ld", jackFramesAvailable);

		/* Now that we have finished filling the buffer, either until
		 * it is full or until we have run out of application sound
		 * data to process, output the audio to the jack server and
		 * apply volume to the floating point output sound buffer */
		XMMS_CALLBACK_DBG ("appling volume of ");

		for (i = 0; i < data->num_output_channels; i++) {
			XMMS_CALLBACK_DBG ("\t%f to channel %d ", data->volume[i], i);
		}

		XMMS_CALLBACK_DBG ("to %ld frames and %ld channels",
		                   (nframes - jackFramesAvailable),
		                   data->num_output_channels);

		xmms_jack_float_volume_effect (out_buffer,
		                               (nframes - jackFramesAvailable),
		                               data->num_output_channels,
		                               data->volume);

		/* see if we still have jackBytesLeft here, if we do that means
		 * that we ran out of wave data to play and had a buffer underrun,
		 * fill in the rest of the space with zero bytes so at least there
		 * is silence */
		if (jackFramesAvailable) {
			XMMS_CALLBACK_DBG ("buffer underrun of %ld frames",
			                   jackFramesAvailable);

			for (i = 0 ; i < data->num_output_channels; i++) {
				jack_default_audio_sample_t *dest;

				dest = out_buffer[i] + (nframes - jackFramesAvailable);

				xmms_jack_sample_silence_ds (dest, jackFramesAvailable);
			}
		}
	}
	else if (data->state == PAUSED || data->state == STOPPED ||
	         data->state == CLOSED || data->state == RESET) {

		XMMS_CALLBACK_DBG ("PAUSED or STOPPED or CLOSED, outputting silence");

		/* output silence if nothing is being outputted */
		for (i = 0; i < data->num_output_channels; i++) {
			xmms_jack_sample_silence_ds (out_buffer[i], nframes);
		}

		/* if we were told to reset then zero out some variables */
		/* and transition to STOPPED */
		if (data->state == RESET) {

			/* transition to STOPPED */
			data->state = STOPPED;

		}
	}

	XMMS_CALLBACK_DBG ("callback done");

	/* free output buffer ports */
	g_free (out_buffer);

	return 0;
}

/**
 * Callback when the server changes the number of frames.
 *
 * @param nframes new number of frames per callback
 * @param arg pointer to xmms output structure
 * @return always zero
 */
static gint
JACK_bufsize (jack_nframes_t nframes, void *arg)
{
	xmms_jack_data_t* data;
	gulong buffer_required;

	data = xmms_output_private_data_get ((xmms_output_t *) arg);

	XMMS_DBG ("the maximum buffer size is now %lu frames",
	          (gulong) nframes);

	/* make sure the callback routine has adequate memory for the nframes
	 * it will get ie. Buffer_size < (bytes we already wrote +
	 *                          bytes we are going to write in this loop) */

	/* frames * sizeof(gfloat) * X channels of output */
	buffer_required = nframes * sizeof (gfloat) * data->num_output_channels;
	if (data->buffer_size < buffer_required) {

		XMMS_DBG ("expanding buffer from data->buffer_size == %ld, to %ld",
		          data->buffer_size, buffer_required);

		data->buffer_size = buffer_required;
		data->sound_buffer = g_realloc (data->sound_buffer, data->buffer_size);

		/* if we don't have a buffer then error out */
		if (!data->sound_buffer) {
			xmms_log_fatal ("error allocating sound_buffer memory");
			return 0;
		}
	}

	XMMS_DBG ("JACK_bufsize called");

	return 0;
}


/**
 * Callback that occurs when jacks sample rate changes.
 *
 * @param nframes new sample rate in frames per-second
 * @param arg pointer to the xmms output structure
 * @return always zero
 */
static gint
JACK_srate (jack_nframes_t nframes, void *arg)
{
	XMMS_DBG ("the sample rate is now %lu/sec", (gulong) nframes);
	return 0;
}


/**
 * Callback that is called when jack is shutting down.
 *
 * @param arg void pointer to xmms output structure
 */
static void
JACK_shutdown (void *arg)
{
	xmms_jack_data_t *data;
	xmms_output_t *output = (xmms_output_t*) arg;
	xmms_error_t error;

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	data->client = 0; /* reset client */

	XMMS_DBG ("trying to reconnect to jack");

	/* lets see if we can't reestablish the connection */
	if (xmms_jack_open_device (output) != ERR_SUCCESS) {
		xmms_log_error ("unable to reconnect with jack...");
		xmms_error_set (&error, XMMS_ERROR_GENERIC,
		                "Jack shutdown, unable to reconnect...");
		xmms_output_set_error (output, &error);
	}
}


/**
 * Callback that jack calls if an error occurs.
 *
 * @param desc ascii text description of error
 */
static void
JACK_Error (const gchar *desc)
{
	XMMS_DBG ("JACK_Error() %s", desc);
}


/**
 * Set an internal variable to tell JACK_Callback() to reset this device
 * when the next callback occurs.
 *
 * @param data pointer to a xmms_jack_data_t structure
 */
void
xmms_jack_reset (xmms_jack_data_t *data)
{
	XMMS_DBG ("xmms_jack_reset() resetting this of %p", data);

	/* NOTE: we use the RESET state so we don't need to worry about
	 * clearing out variables that the callback modifies while the
	 * callback is running we set the state to RESET and the callback
	 * clears the variables out for us */

	/* tell the callback that we are to reset, the callback will
	 * transition this to STOPPED */
	data->state = RESET;
}


/**
 * Free the array of channel names.
 *
 * @param data pointer to a xmms_jack_data_t structure
 */
static void
xmms_jack_free_channel_names (xmms_jack_data_t *data)
{
	/* free up the existing channel names */
	if (data->channel_names) {
		guint x;
		for (x = 0; data->channel_names[x]; x++) {
			g_free (data->channel_names[x]);
		}
		g_free (data->channel_names);

		data->channel_names = NULL;
	}
}


/**
 * Create an array of channel names.
 *
 * @param data pointer to a xmms_jack_data_t structure
 */
static void
xmms_jack_create_channel_names (xmms_jack_data_t *data)
{
	guint x;

	/* assign channel names to each of the channels */
	data->channel_names = g_new (gchar *, data->num_output_channels + 1);

	for (x = 0; x < data->num_output_channels; x++) {
		gchar channel_name[32];
		g_snprintf (channel_name, sizeof (channel_name), "port%d", x);
		data->channel_names[x] = g_strdup (channel_name);
	}

	/* null terminate the string list */
	data->channel_names[data->num_output_channels] = NULL;
}


/**
 * Close a jack connection cleanly.
 *
 * @param output The output structure.
 */
static void
xmms_jack_close_device (xmms_output_t *output)
{
	xmms_jack_data_t *data;

	XMMS_DBG ("xmms_jack_close_device() closing the jack client thread");

	data = xmms_output_private_data_get (output);

	if (data->client) {
		XMMS_DBG ("after jack_deactivate()");
		jack_client_close (data->client);
	}

	xmms_jack_reset (data);

	/* reset client */
	data->client = NULL;

	/* free buffer memory */
	g_free (data->sound_buffer);

	data->sound_buffer = NULL;

	/* zero out size of the buffer */
	data->buffer_size  = 0;

	/* free up the output_port array */
	if (data->output_port) {
		g_free (data->output_port);
		data->output_port = NULL;
	}

	/* free the volume array */
	if (data->volume) {
		g_free (data->volume);
		data->volume = NULL;
	}

	/* free the channel names */
	xmms_jack_free_channel_names (data);
}


/**
 * Open a jack device.
 *
 * @param output The output structure.
 * @return success status
 */
static gint
xmms_jack_open_device (xmms_output_t *output)
{
	xmms_jack_data_t *data;
	const gchar** ports;
	gint i, ret;
	gchar client_name[64];
	gint failed = 0;

	XMMS_DBG ("creating jack client and setting up callbacks");

	data = xmms_output_private_data_get (output);

	/* see if this device is already open */
	if (data->client) {
		return ERR_OPENING_JACK;
	}

	/* zero out the buffer pointer and the size of the buffer */
	data->sound_buffer = NULL;
	data->buffer_size = 0;

	/* set up an error handler */
	jack_set_error_function (JACK_Error);

	/* try to become a client of the JACK server */
	srand (time (NULL));
	g_snprintf (client_name, sizeof (client_name),
	            "xmms_jack_%d_%d", 0, rand ());

	XMMS_DBG ("client name '%s'", client_name);

	data->client = jack_client_new (client_name);
	if (!data->client) {
		/* jack has problems with shutting down clients, so lets
		 * wait a short while and try once more before we give up */
		data->client = jack_client_new (client_name);
		if (!data->client) {
			XMMS_DBG ("unable to jack_client_new(), jack server not running?");
			return ERR_OPENING_JACK;
		}
	}

	/* JACK server to call `xmms_jack_callback()' whenever
	 * there is work to be done. */
	jack_set_process_callback (data->client, xmms_jack_callback, output);

	/* setup a buffer size callback */
	jack_set_buffer_size_callback (data->client, JACK_bufsize, output);

	/* tell the JACK server to call `srate()' whenever
	   the sample rate of the system changes. */
	jack_set_sample_rate_callback (data->client, JACK_srate, output);

	/* tell the JACK server to call `jack_shutdown()' if
	 * it ever shuts down, either entirely, or if it
	 * just decides to stop calling us. */
	jack_on_shutdown (data->client, JACK_shutdown, output);

	/* display the current sample rate. once the client is activated
	 * (see below), you should rely on your own sample rate
	 * callback (see above) for this value. */
	data->sample_rate = jack_get_sample_rate (data->client);
	XMMS_DBG ("engine sample rate: %lu", data->sample_rate);

	/* free, if allocated, and allocate memory for the output ports */
	if (data->output_port) {
		g_free (data->output_port);
		data->output_port = 0;
	}

	data->output_port = g_new (jack_port_t *, data->num_output_channels);

	/* create the output ports */
	for (i = 0; i < data->num_output_channels; i++) {
		gchar portname[32];
		g_snprintf (portname, sizeof (portname), "out_%d", i);
		XMMS_DBG ("port %d is named '%s'", i, portname);
		data->output_port[i] = jack_port_register (data->client, portname,
		                                           JACK_DEFAULT_AUDIO_TYPE,
		                                           JackPortIsOutput, 0);
	}

	/* set the initial buffer size */
	JACK_bufsize (jack_get_buffer_size (data->client), output);

	/* tell the JACK server that we are ready to roll */
	if (jack_activate (data->client)) {
		xmms_log_fatal ( "cannot activate client");
		return ERR_OPENING_JACK;
	}


	XMMS_DBG ("jack_get_ports() passing in NULL/NULL");
	ports = jack_get_ports (data->client, NULL, NULL, JackPortIsInput);


	/* display a trace of the output ports we found */
	for (i = 0; ports[i]; i++) {
		XMMS_DBG ("ports[%d] = '%s'", i, ports[i]);
	}

	/* see if we have enough ports */
	if (i < data->num_output_channels) {
		XMMS_DBG ("ERR: jack_get_ports() failed to find "
		          "ports with jack port flags of 0x%X'", JackPortIsInput);
		return ERR_PORT_NOT_FOUND;
	}

	/* connect the ports. Note: you can't do this before
	 * the client is activated (this may change in the future). */
	for (i = 0; i < data->num_output_channels; i++) {
		XMMS_DBG ("jack_connect() to port '%p'", data->output_port[i]);

		ret = jack_connect (data->client,
		                    jack_port_name (data->output_port[i]), ports[i]);
		if (ret) {
			xmms_log_fatal ("cannot connect to output port %d('%s')", i,
			                ports[i]);
			failed = 1;
		}
	}

	g_free (ports); /* free the returned array of ports */

	/* free any existing volume structure that might exist
	 * we could have created it with a different number of
	 * output channels */
	if (data->volume) {
		g_free (data->volume);
		data->volume = 0;
	}

	/* allocate space for the volume of each channel */
	data->volume = g_new (gfloat, data->num_output_channels);

	/* free the old channel names */
	xmms_jack_free_channel_names (data);

	/* create new channel names */
	xmms_jack_create_channel_names (data);

	/* if something failed we need to shut
	 * the client down and return 0 */
	if (failed) {
		xmms_jack_close_device (output);
		return ERR_OPENING_JACK;
	}

	return ERR_SUCCESS; /* return success */
}


/**
 * Handle the non-jack related aspects of opening up the device.
 *
 * @param output The output structure.
 * @param bytes_per_channel bytes per channel
 * @param rate pointer to the requested rate, will be set to jack's rate
 * @param channels number of channels
 * @return success status
 */
static gint
xmms_jack_open (xmms_output_t *output, guint bytes_per_channel,
                gulong *rate, gint channels)
{
	gint output_channels, input_channels;
	glong bytes_per_output_frame;
	glong bytes_per_input_frame;
	xmms_jack_data_t *data;
	gint retval;

	data = xmms_output_private_data_get (output);

	output_channels = input_channels = channels;

	/* flushes all queued buffers, sets status
	 * to STOPPED and resets some variables */
	xmms_jack_reset (data);

	/* data->sample_rate is set by xmms_jack_open_device() */
	data->num_input_channels = input_channels;
	data->num_output_channels = output_channels;
	bytes_per_input_frame = (bytes_per_channel * data->num_input_channels);
	bytes_per_output_frame = (bytes_per_channel * data->num_output_channels);

	XMMS_DBG ("num_input_channels == %ld", data->num_input_channels);
	XMMS_DBG ("num_output_channels == %ld", data->num_output_channels);
	XMMS_DBG ("bytes_per_output_frame == %ld", bytes_per_output_frame);
	XMMS_DBG ("bytes_per_input_frame  == %ld", bytes_per_input_frame);

	/* make sure bytes_per_frame is valid and non-zero */
	if (!bytes_per_output_frame) {
		xmms_log_fatal ("bytes_per_output_frame is zero");
		return ERR_BYTES_PER_OUTPUT_FRAME_INVALID;
	}

	/* make sure bytes_per_frame is valid and non-zero */
	if (!bytes_per_input_frame) {
		xmms_log_fatal ("bytes_per_output_frame is zero");
		return ERR_BYTES_PER_INPUT_FRAME_INVALID;
	}

	/* go and open up the device */
	retval = xmms_jack_open_device (output);
	if (retval != ERR_SUCCESS) {
		XMMS_DBG ("error opening jack device");
		return retval;
	} else {
		XMMS_DBG ("succeeded opening jack device");
	}

	/* make sure the sample rate of the jack server
	 * matches that of the client */
	if ((glong)(*rate) != data->sample_rate) {
		XMMS_DBG ("rate of %ld doesn't match jack sample rate of %ld,"
		          "returning error", *rate, data->sample_rate);

		*rate = data->sample_rate;
		xmms_jack_close_device (output);

		return ERR_RATE_MISMATCH;
	}

	return ERR_SUCCESS; /* success */
}


/**
 * Set volume.
 *
 * @param output The output structure.
 * @param channel input channel name string
 * @param volume input volume level(range of 0 to 100)
 * @return TRUE on success
 */
static gboolean
xmms_jack_volume_set (xmms_output_t *output, const gchar *channel,
                      guint volume)
{
	xmms_jack_data_t *data;
	guchar x;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	/* find the channel whos volume we should change */
	for (x = 0; x < data->num_output_channels; x++) {
		if (g_strcasecmp (channel, data->channel_names[x]) == 0) {
			data->volume[x] = (gfloat) volume / 100.0;
			return TRUE;
		}
	}

	return FALSE;
}


/**
 * Get current volume.
 *
 * @param output The output structure.
 * @param names channel names
 * @param values channel values
 * @param num_channels number of channels
 * @return TRUE on success
 */
static gboolean
xmms_jack_volume_get (xmms_output_t *output,
                      const gchar **names, guint *values,
                      guint *num_channels)
{
	xmms_jack_data_t *data;
	guint x;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	/* (*num_channels) of 0 indicates that the caller is requesting the
	 * number of channels we have */
	if (!*num_channels) {
		*num_channels = data->num_output_channels;
		return TRUE;
	}

	g_return_val_if_fail (names, FALSE);
	g_return_val_if_fail (values, FALSE);

	if (!data->volume) {
		return FALSE;
	}

	for (x = 0; x < data->num_output_channels; x++) {
		values[x] = (guint) data->volume[x] * 100;
		names[x] = data->channel_names[x];
	}

	return TRUE;
}


/**
 * Flush the audio output, doesn't apply as we don't buffer any
 * audio data.
 *
 * @param output The output structure.
 */
static void
xmms_jack_flush (xmms_output_t *output)
{
	XMMS_DBG ("xmms_jack_flush called");
}


/**
 * Create a new jack object.
 *
 * @param output The output structure.
 * @return TRUE on success
 */
static gboolean
xmms_jack_new (xmms_output_t *output)
{
	xmms_jack_data_t *data;

	XMMS_DBG ("xmms_jack_new");

	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_jack_data_t, 1);

	xmms_output_private_data_set (output, data);

	return xmms_jack_start (output);
}


/**
 * Frees the plugin data allocated in xmms_jack_new()
 * and closes the connection to jack.
 *
 * @param output The output structure.
 */
static void
xmms_jack_destroy (xmms_output_t *output)
{
	xmms_jack_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	/* if playing, stop and close the device */
	if (data->state == PLAYING) {
		data->state = STOPPED;
		xmms_jack_close_device (output);
	}

	g_free (data);
}


/**
 * Get buffersize.
 *
 * @param output The output structure.
 * @return the current buffer size or 0 on failure.
 */
static guint
xmms_jack_buffersize_get (xmms_output_t *output)
{
	xmms_jack_data_t *data;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	return data->buffer_size;
}


/**
 * Initialize jack output.
 *
 * @param output The output structure.
 * @return TRUE if a connection to the jack sound daemon was established
 */
static gboolean
xmms_jack_start (xmms_output_t *output)
{
	xmms_jack_data_t *data;
	gulong outputFrequency = 0;
	gint bytes_per_sample = 2;
	gint channels = 2; /* @todo stop hardcoding 2 channels here */
	guint ret;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	XMMS_DBG ("xmms_jack_start");

	/* if we are already open, just return true */
	if (data->client) {
		return TRUE;
	}

	ret = xmms_jack_open (output, bytes_per_sample,
	                      &outputFrequency, channels);

	if (ret == ERR_RATE_MISMATCH) {
		XMMS_DBG ("we want a rate of '%ld', opening at jack rate",
		          outputFrequency);

		/* open the jack device with true jack's rate, return 0 upon failure */
		ret = xmms_jack_open (output, bytes_per_sample,
		                      &outputFrequency, channels);

		if (ret) {
			xmms_log_error ("failed to open jack with xmms_jack_open(),"
			                "error %d", ret);
			return FALSE;
		}

		XMMS_DBG ("success!!");
	} else if (ret != ERR_SUCCESS) {
		xmms_log_error ("failed to open jack with xmms_jack_open(), error %d",
		                ret);
		return FALSE;
	}

	data->rate = outputFrequency;

	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_FLOAT,
	                        channels, outputFrequency);

	XMMS_DBG ("jack started!!");

	return TRUE;
}


/**
 * Callback from xmms whenever the playback state of the plugin changes.
 *
 * @param output The output structure.
 * @param status requested status
 * @return TRUE on success
 */
static gboolean
xmms_jack_status (xmms_output_t *output, xmms_playback_status_t status)
{
	xmms_jack_data_t *data;
	const gchar *tmp;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		tmp = "PLAYING";
	} else {
		tmp = "STOPPED";
	}
	XMMS_DBG ("changed status! '%s'", tmp);

	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		if (!xmms_jack_start (output)) {
			xmms_log_error ("unable to start jack with jack_start(),"
			                "is jack server running?");
			return FALSE;
		}

		data->state = PLAYING;
	} else {
		data->state = STOPPED;
	}

	return TRUE;
}
