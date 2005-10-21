/*      xmms2 - jack output plugin
 *    Copyright (C) 2004  Chris Morgan <cmorgan@alum.wpi.edu>
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
 */

#include "xmms/xmms_defs.h"
#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#include <glib.h>
#include <stdlib.h>
#include <unistd.h>
#include <jack/jack.h>
#include <sys/time.h>

#define min(a,b)   (((a) < (b)) ? (a) : (b))

#define MAX_OUTPUT_PORTS  10

#define ERR_SUCCESS                           0
#define ERR_OPENING_JACK                      1
#define ERR_RATE_MISMATCH                     2
#define ERR_BYTES_PER_OUTPUT_FRAME_INVALID    3
#define ERR_BYTES_PER_INPUT_FRAME_INVALID     4
#define ERR_TOO_MANY_OUTPUT_CHANNELS          5
#define ERR_PORT_NAME_OUTPUT_CHANNEL_MISMATCH 6
#define ERR_PORT_NOT_FOUND                    7

enum status_enum { PLAYING, PAUSED, STOPPED, CLOSED, RESET };

typedef struct xmms_jack_data_St {
	guint               rate;
	gboolean            have_mixer;
	xmms_config_value_t *mixer_conf;

	unsigned char*      sound_buffer;                  /* temporary buffer used to process data before sending to jack */

	jack_port_t*        output_port[MAX_OUTPUT_PORTS]; /* output ports */
	jack_client_t*      client;                        /* pointer to jack client */
	enum status_enum    state;                         /* one of PLAYING, PAUSED, STOPPED, CLOSED, RESET etc*/

	float               volume[MAX_OUTPUT_PORTS];
	long                sample_rate;                   /* samples(frames) per second */
	unsigned long       num_input_channels;            /* number of input channels(1 is mono, 2 stereo etc..) */
	unsigned long       num_output_channels;           /* number of output channels(1 is mono, 2 stereo etc..) */

	unsigned long       buffer_size;                   /* number of bytes in the buffer allocated for processing data in JACK_Callback */
} xmms_jack_data_t;


static int
_JACK_OpenDevice(xmms_output_t *output);
static int
_JACK_Open(xmms_output_t *output, unsigned int bytes_per_channel, unsigned long *rate, int channels);



#define CALLBACK_TRACE     0


#if CALLBACK_TRACE
#define XMMS_CALLBACK_DBG      XMMS_DBG
#else
#define XMMS_CALLBACK_DBG(...) do {} while(0)
#endif



static gboolean xmms_jack_mixer_set(xmms_output_t *output, gint l, gint r);

/**
 * floating point volume routine
 * volume should be a value between 0.0 and 1.0
 * @param sample buffer to apply volume adjustment to
 * @param number of samples to process
 * @param value to apply to these samples
 */
static void
_JACK_float_volume_effect(jack_default_audio_sample_t **buffer, unsigned long nsamples, 
                          unsigned int output_channels, float* volume)
{
    unsigned int x;

    jack_default_audio_sample_t *buf;
    unsigned long samples;
    float vol;

    /* process each output channel */
    for(x = 0; x < output_channels; x++)
    {
        buf = buffer[x];
        samples = nsamples;
        vol = volume[x];

        if(vol < 0)   vol = 0;
        if(vol > 1.0) vol = 1.0;

        while (samples--)
        {
            *buf = (*buf) * vol;
            buf++;
        }    
    }
}


/**
 * fill dst buffer with nsamples worth of silence
 * @param buffer to fill with silence
 * @param number of samples of silence to fill the buffer with
 */
static void
_JACK_sample_silence_dS (jack_default_audio_sample_t *dst, unsigned long nsamples)
{
	/* ALERT: signed sign-extension portability !!! */
	while (nsamples--)
	{
		*dst = 0;
		dst++;
	}
}


/**
 * Main callback, jack calls this function anytime it needs new data
 * @param number of frames of data that jack wants
 * @param pointer to xmms output structure
 */
static int
JACK_callback (jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t* out_buffer[MAX_OUTPUT_PORTS];
	xmms_jack_data_t *this;
	int i;
	xmms_output_t *output = (xmms_output_t*)arg;

	XMMS_CALLBACK_DBG("nframes %ld, sizeof(jack_default_audio_sample_t) == %d", (long)nframes,
	    sizeof(jack_default_audio_sample_t));

	this = xmms_output_private_data_get (output);

	if(!this->client)
		xmms_log_fatal("client is closed, this is weird...");

	XMMS_CALLBACK_DBG("num_output_channels = %ld, num_input_channels = %ld",
	    this->num_output_channels, this->num_input_channels);

	/* retrieve the buffers for the output ports */
	for(i = 0; i < this->num_output_channels; i++)
		out_buffer[i] = (jack_default_audio_sample_t *) jack_port_get_buffer(this->output_port[i], nframes);

	/* handle playing state */
	if(this->state == PLAYING)
	{
		unsigned long jackFramesAvailable = nframes; /* frames we have left to write to jack */
		long inputFramesAvailable;                   /* frames we have available this loop */
		unsigned long numFramesToWrite;              /* num frames we are writing this loop */

		XMMS_CALLBACK_DBG("playing... jackFramesAvailable = %ld", jackFramesAvailable);

		/* see if our buffer is large enough for the data we are writing */
		/* ie. Buffer_size < (bytes we already wrote + bytes we are going to write in this loop) */
		/* Note: sound_buffer is always filled with 16-bit data */
		/* so frame * 2 bytes(16 bits) * X output channels */
		if(this->buffer_size < (jackFramesAvailable * sizeof(float) * this->num_output_channels))
		{
			XMMS_DBG("our buffer must have changed size");
			xmms_log_fatal("allocated %ld bytes, need %ld bytes", this->buffer_size,
				       jackFramesAvailable * sizeof(float) * this->num_output_channels);
			return 0;
		}

		XMMS_CALLBACK_DBG("trying to read %ld bytes\n", jackFramesAvailable * sizeof(float) * this->num_input_channels);

		inputFramesAvailable = xmms_output_read(output, (gchar *)this->sound_buffer, jackFramesAvailable * sizeof(float) * this->num_input_channels);
		if(inputFramesAvailable == -1) inputFramesAvailable = 0;

		inputFramesAvailable = inputFramesAvailable / (sizeof(float) * this->num_input_channels);

		XMMS_CALLBACK_DBG("inputFramesAvailable == %ld, jackFramesAvailable == %ld", inputFramesAvailable, jackFramesAvailable);

		numFramesToWrite = min(jackFramesAvailable, inputFramesAvailable); /* write as many bytes as we have space remaining, or as much as we have data to write */

		XMMS_CALLBACK_DBG("nframes == %ld, jackFramesAvailable == %ld,\n\tthis->num_input_channels == %ld, this->num_output_channels == %ld",
				  (long)nframes, jackFramesAvailable, this->num_input_channels, this->num_output_channels);

		for(i = 0; i < numFramesToWrite; i++)
		{
			out_buffer[0][i] = ((float*)this->sound_buffer)[i*2];
			out_buffer[1][i] = ((float*)this->sound_buffer)[i*2+1];
		}

		jackFramesAvailable-=numFramesToWrite; /* take away what was written */

		XMMS_CALLBACK_DBG("jackFramesAvailable == %ld", jackFramesAvailable);

		/* Now that we have finished filling the buffer either until it is full or until */
		/* we have run out of application sound data to process, output */
		/* the audio to the jack server */

		/* apply volume to the floating point output sound buffer */
		XMMS_CALLBACK_DBG("applying volume of %f, %f to %ld frames and %ld channels",
				  this->volume[0], this->volume[1],
				  (nframes - jackFramesAvailable), this->num_output_channels);
		_JACK_float_volume_effect(out_buffer, (nframes - jackFramesAvailable),
				    this->num_output_channels, this->volume);

		/* see if we still have jackBytesLeft here, if we do that means that we
		   ran out of wave data to play and had a buffer underrun, fill in
		   the rest of the space with zero bytes so at least there is silence */
		if(jackFramesAvailable)
		{
			XMMS_CALLBACK_DBG("buffer underrun of %ld frames", jackFramesAvailable);
			for(i = 0 ; i < this->num_output_channels; i++)
				_JACK_sample_silence_dS(out_buffer[i] + (nframes - jackFramesAvailable),
							jackFramesAvailable);
		}
	}
	else if(this->state == PAUSED ||
		this->state == STOPPED ||
		this->state == CLOSED || this->state == RESET)
	{
		XMMS_CALLBACK_DBG("PAUSED or STOPPED or CLOSED, outputting silence");

		/* output silence if nothing is being outputted */
		for(i = 0; i < this->num_output_channels; i++)
			_JACK_sample_silence_dS(out_buffer[i], nframes);

		/* if we were told to reset then zero out some variables */
		/* and transition to STOPPED */
		if(this->state == RESET)
			this->state = STOPPED; /* transition to STOPPED */
	}

	XMMS_CALLBACK_DBG("callback done");

	return 0;
}

/**
 * Callback when the server changes the number of frames
 * @param new number of frames per callback
 * @param pointer to xmms output structure
 */
static int
JACK_bufsize (jack_nframes_t nframes, void *arg)
{
	xmms_jack_data_t* this;
	unsigned long buffer_required;

	this = xmms_output_private_data_get((xmms_output_t*)arg);

	XMMS_DBG("the maximum buffer size is now %lu frames", (unsigned long)nframes);

	/* make sure the callback routine has adequate memory for the nframes it will get */
	/* ie. Buffer_size < (bytes we already wrote + bytes we are going to write in this loop) */
	/* frames * sizeof(float) * X channels of output */
	buffer_required = nframes * sizeof(float) * this->num_output_channels;
	if(this->buffer_size < buffer_required)
	{
		XMMS_DBG("expanding buffer from this->buffer_size == %ld, to %ld",
			 this->buffer_size, buffer_required);
		this->buffer_size = buffer_required;
		this->sound_buffer = realloc(this->sound_buffer, this->buffer_size);

		/* if we don't have a buffer then error out */
		if(!this->sound_buffer)
		{
			xmms_log_fatal("error allocating sound_buffer memory");
			return 0;
		}
	}

	XMMS_DBG("JACK_bufsize called");
	return 0;
}

/**
 * Callback that occurs when jacks sample rate changes
 * @param new sample rate in frames per-second
 * @param pointer to the xmms output structure
 */
static int
JACK_srate (jack_nframes_t nframes, void *arg)
{
	XMMS_DBG("the sample rate is now %lu/sec", (unsigned long)nframes);
	return 0;
}

/**
 * Callback that is called when jack is shutting down
 * @param void pointer to xmms output structure
 */
static void
JACK_shutdown(void* arg)
{
	xmms_jack_data_t *this;
	xmms_output_t *output = (xmms_output_t*)arg;

	this = xmms_output_private_data_get (output);
	g_return_if_fail (this);

	this->client = 0; /* reset client */

	XMMS_DBG("trying to reconnect to jack");

	/* lets see if we can't reestablish the connection */
	if(_JACK_OpenDevice(output) != ERR_SUCCESS)
	{
		xmms_log_fatal("unable to reconnect with jack...");
	}
}


/**
 * Callback that jack calls if an error occurs
 * @param ascii text description of error
 */
static void
JACK_Error(const char *desc)
{
	XMMS_DBG("JACK_Error() %s", desc);
}


/**
 * Set an internal variable to tell JACK_Callback() to reset this device
 * when the next callback occurs
 * @param pointer to a xmms_jack_data_t structure
 */
void
_JACK_reset(xmms_jack_data_t *this)
{
	XMMS_DBG("_JACK_reset() resetting this of %p", this);

	/* NOTE: we use the RESET state so we don't need to worry about clearing out */
	/* variables that the callback modifies while the callback is running */
	/* we set the state to RESET and the callback clears the variables out for us */
	this->state = RESET; /* tell the callback that we are to reset, the callback will transition this to STOPPED */
}

/**
 * Close a jack connection cleanly
 * @param output xmms object
 */
static void
_JACK_CloseDevice(xmms_output_t *output)
{
	xmms_jack_data_t *this;
	XMMS_DBG("_JACK_CloseDevice() closing the jack client thread");

	this = xmms_output_private_data_get(output);

	if(this->client)
	{
		XMMS_DBG("after jack_deactivate()");
		jack_client_close(this->client);
	}

	_JACK_reset(this);
	this->client       = 0; /* reset client */
	free(this->sound_buffer); /* free buffer memory */
	this->sound_buffer = 0;
	this->buffer_size  = 0; /* zero out size of the buffer */
}

/**
 * Open a jack device
 * @param output xmms object
 */
static int
_JACK_OpenDevice(xmms_output_t *output)
{
	xmms_jack_data_t *data;
	const char** ports;
	int i;
	char client_name[64];
	int failed = 0;

	XMMS_DBG("creating jack client and setting up callbacks");

	data = xmms_output_private_data_get(output);

	/* see if this device is already open */
	if(data->client)
		return ERR_OPENING_JACK;

	/* zero out the buffer pointer and the size of the buffer */
	data->sound_buffer = 0;
	data->buffer_size = 0;

	/* set up an error handler */
	jack_set_error_function(JACK_Error);

	/* try to become a client of the JACK server */
	srand (time (NULL));
	g_snprintf(client_name, sizeof(client_name), "xmms_jack_%d_%d", 0, rand());
	XMMS_DBG("client name '%s'", client_name);
	if ((data->client = jack_client_new(client_name)) == 0)
	{
		/* jack has problems with shutting down clients, so lets */
		/* wait a short while and try once more before we give up */
		if ((data->client = jack_client_new(client_name)) == 0)
		{
			xmms_log_fatal("jack server not running?");
			return ERR_OPENING_JACK;
		}
	}

	/* JACK server to call `JACK_callback()' whenever
	   there is work to be done. */
	jack_set_process_callback(data->client, JACK_callback, output);

	/* setup a buffer size callback */
	jack_set_buffer_size_callback(data->client, JACK_bufsize, output);

	/* tell the JACK server to call `srate()' whenever
	   the sample rate of the system changes. */
	jack_set_sample_rate_callback(data->client, JACK_srate, output);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us. */
	jack_on_shutdown(data->client, JACK_shutdown, output);

	/* display the current sample rate. once the client is activated
	   (see below), you should rely on your own sample rate
	   callback (see above) for this value. */
	data->sample_rate = jack_get_sample_rate(data->client);
	XMMS_DBG("engine sample rate: %lu", data->sample_rate);

	/* create the output ports */
	for(i = 0; i < data->num_output_channels; i++)
	{
		char portname[32];
		g_snprintf(portname, 32, "out_%d", i);
		XMMS_DBG("port %d is named '%s'", i, portname);
		data->output_port[i] = jack_port_register(data->client, portname,
							  JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	}

	/* set the initial buffer size */
	JACK_bufsize(jack_get_buffer_size(data->client), output);

	/* tell the JACK server that we are ready to roll */
	if(jack_activate(data->client))
	{
		xmms_log_fatal( "cannot activate client");
		return ERR_OPENING_JACK;
	}


	XMMS_DBG("jack_get_ports() passing in NULL/NULL");
	ports = jack_get_ports(data->client, NULL, NULL, JackPortIsInput);


	/* display a trace of the output ports we found */
	for(i = 0; ports[i]; i++)
		XMMS_DBG("ports[%d] = '%s'", i, ports[i]);

	/* see if we have enough ports */
	if(i < data->num_output_channels)
	{
		XMMS_DBG("ERR: jack_get_ports() failed to find ports with jack port flags of 0x%X'", JackPortIsInput);
		return ERR_PORT_NOT_FOUND;
	}

	/* connect the ports. Note: you can't do this before
	   the client is activated (this may change in the future). */
	for(i = 0; i < data->num_output_channels; i++)
	{
		XMMS_DBG("jack_connect() to port '%p'", data->output_port[i]);
		if(jack_connect(data->client, jack_port_name(data->output_port[i]), ports[i]))
		{
			xmms_log_fatal("cannot connect to output port %d('%s')", i, ports[i]);
			failed = 1;
		}
	} 

	free(ports); /* free the returned array of ports */

	/* if something failed we need to shut the client down and return 0 */
	if(failed)
	{
		_JACK_CloseDevice(output);
		return ERR_OPENING_JACK;
	}

	return ERR_SUCCESS; /* return success */
}

/**
 * Handle the non-jack related aspects of opening up the device
 * @param output xmms object
 * @param bytes per channel
 * @param pointer to the requested rate, will be set to jack's rate
 * @param number of channels
 */
static int
_JACK_Open(xmms_output_t *output, unsigned int bytes_per_channel,
	   unsigned long *rate, int channels)
{ 
	int retval;
	int output_channels, input_channels;
	long bytes_per_output_frame;
	long bytes_per_input_frame;
	xmms_jack_data_t *this;

	this = xmms_output_private_data_get(output);

	output_channels = input_channels = channels;

	_JACK_reset(this); /* flushes all queued buffers, sets status to STOPPED and resets some variables */

	/* this->sample_rate is set by _JACK_OpenDevice() */
	this->num_input_channels     = input_channels;
	this->num_output_channels    = output_channels;
	bytes_per_input_frame  = (bytes_per_channel*this->num_input_channels);
	bytes_per_output_frame = (bytes_per_channel*this->num_output_channels);

	XMMS_DBG("num_input_channels == %ld", this->num_input_channels);
	XMMS_DBG("num_output_channels == %ld", this->num_output_channels);
	XMMS_DBG("bytes_per_output_frame == %ld", bytes_per_output_frame);
	XMMS_DBG("bytes_per_input_frame  == %ld", bytes_per_input_frame);

	/* make sure bytes_per_frame is valid and non-zero */
	if(!bytes_per_output_frame)
	{
		xmms_log_fatal("bytes_per_output_frame is zero");
		return ERR_BYTES_PER_OUTPUT_FRAME_INVALID;
	}

	/* make sure bytes_per_frame is valid and non-zero */
	if(!bytes_per_input_frame)
	{
		xmms_log_fatal("bytes_per_output_frame is zero");
		return ERR_BYTES_PER_INPUT_FRAME_INVALID;
	}

	/* go and open up the device */
	retval = _JACK_OpenDevice(output);
	if(retval != ERR_SUCCESS)
	{
		XMMS_DBG("error opening jack device");
		return retval;
	} else
	{
		XMMS_DBG("succeeded opening jack device");
	}

	/* make sure the sample rate of the jack server matches that of the client */
	if((long)(*rate) != this->sample_rate)
	{
		XMMS_DBG("rate of %ld doesn't match jack sample rate of %ld, returning error",
			 *rate, this->sample_rate);
		*rate = this->sample_rate;
		_JACK_CloseDevice(output);
		return ERR_RATE_MISMATCH;
	}

	return ERR_SUCCESS; /* success */
}


/**
 * Set volume
 * @param output xmms object
 * @param input left volume level(range of 0 to 100)
 * @param input right volume level(range of 0 to 100)
 */
static gboolean
xmms_jack_mixer_set(xmms_output_t *output, gint l, gint r)
{
	xmms_jack_data_t *data;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	XMMS_DBG("l(%d), r(%d)", l, r);
	data->volume[0] = (float)l / 100.0;
	data->volume[1] = (float)r / 100.0;

	return TRUE;
}

/**
 * Flush the audio output, doesn't apply as we don't buffer any
 * audio data
 * @param output xmms object
 */
static void
xmms_jack_flush(xmms_output_t *output)
{
	XMMS_DBG("xmms_jack_flush called");
	return;
}

/**
 * Handles config change requests.
 * @param output xmms object
 * @param data the new volume values
 * @param userdata
 */
static void
xmms_jack_mixer_config_changed (xmms_object_t *object, gconstpointer data,
				gpointer userdata)
{
	xmms_jack_data_t *jack_data;
	gchar **tmp;
	guint left, right;
	xmms_output_t *output;
	const gchar *newval;

	XMMS_DBG ("xmms_jack_mixer_config_changed");	

	g_return_if_fail (userdata);
	output = userdata;
	g_return_if_fail (data);
	newval = data;
	jack_data = xmms_output_private_data_get (output);
	g_return_if_fail (jack_data);

	if (jack_data->have_mixer)
	{
		tmp = g_strsplit (newval, "/", 2);

		if (tmp[0])
		{
			left  = atoi (tmp[0]);
		}

		if (tmp[1])
		{
			right = atoi (tmp[1]);
		} else
		{
			right = left;
		}

		xmms_jack_mixer_set (output, left, right);

		g_strfreev (tmp);
	}
}

/**
 * Create a new jack object
 * @param output xmms object
 */
static gboolean
xmms_jack_new(xmms_output_t *output)
{
	xmms_jack_data_t *data;
	unsigned long outputFrequency = 0;
	int bytes_per_sample = 2;
	int channels = 2;     /** @todo stop hardcoding 2 channels here */
	int retval;

	XMMS_DBG ("xmms_jack_new"); 

	g_return_val_if_fail (output, FALSE);
	data = g_new0 (xmms_jack_data_t, 1);

	data->mixer_conf = xmms_plugin_config_lookup (
						      xmms_output_plugin_get (output), "volume");

	xmms_config_value_callback_set (data->mixer_conf,
					xmms_jack_mixer_config_changed,
					(gpointer) output);

	xmms_output_private_data_set (output, data); 

	retval = _JACK_Open(output, bytes_per_sample, &outputFrequency, channels);
	if(retval == ERR_RATE_MISMATCH)
	{
		XMMS_DBG("we want a rate of '%ld', opening at jack rate", outputFrequency);

		/* open the jack device with true jack's rate, return 0 upon failure */
		if((retval = _JACK_Open(output, bytes_per_sample, &outputFrequency, channels)))
		{
			XMMS_DBG("failed to open jack with _JACK_Open(), error %d", retval);
			return FALSE;
		}
		XMMS_DBG("success!!");
	} else if(retval != ERR_SUCCESS)
	{
		XMMS_DBG("failed to open jack with _JACK_Open(), error %d", retval);
		return FALSE;
	}

	data->rate = outputFrequency;
	data->volume[0] = .80; /* set volume to 80% */
	data->volume[1] = .80;
	data->state = STOPPED;

	xmms_output_format_add(output,
                           XMMS_SAMPLE_FORMAT_FLOAT,
                           channels, outputFrequency);

	XMMS_DBG("jack initialized!!");

	return TRUE; 
}

/**
 * Frees the plugin data allocated in xmms_jack_new()
 */
static void
xmms_jack_destroy (xmms_output_t *output)
{
	xmms_jack_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	xmms_config_value_callback_remove (data->mixer_conf,
	                                   xmms_jack_mixer_config_changed);

	/* if playing, stop and close the device */
	if(data->state == PLAYING)
	{
		data->state = STOPPED;
		_JACK_CloseDevice(output);
	}

	g_free (data);
}

/**
 * Get buffersize.
 *
 * @param output The output structure.
 * 
 * @return the current buffer size or 0 on failure.
 */
static guint
xmms_jack_buffersize_get(xmms_output_t *output)
{
	xmms_jack_data_t *data;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	return data->buffer_size;
}

/**
 * Callback from xmms whenever the playback state of the plugin changes
 * @param output xmms object
 * @param requested status
 */
static void
xmms_jack_status (xmms_output_t *output, xmms_playback_status_t status)
{
	xmms_jack_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG ("changed status! '%s'", (status == XMMS_PLAYBACK_STATUS_PLAY) ? "PLAYING" : "STOPPED");
	if(status == XMMS_PLAYBACK_STATUS_PLAY)
		data->state = PLAYING;
	else
		data->state = STOPPED;
}

/**
 * Get plugin information
 */
xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, 
				  XMMS_OUTPUT_PLUGIN_API_VERSION,
				  "jack",
				  "jack Output" XMMS_VERSION,
				  "Jack audio server output plugin");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://xmms-jack.sf.net");
	xmms_plugin_info_add (plugin, "Author", "Chris Morgan");
	xmms_plugin_info_add (plugin, "E-Mail", "cmorgan@alum.wpi.edu");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, 
				xmms_jack_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, 
	                        xmms_jack_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, 
				xmms_jack_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET,
				xmms_jack_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_STATUS,
				xmms_jack_status); 

	xmms_plugin_config_value_register (plugin,
					   "volume",
					   "70/70",
					   NULL,
					   NULL);

	return (plugin);
}

