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

#include <glib.h>
#include <stdlib.h>
#include <unistd.h>
#include <jack/jack.h>
#include <sys/time.h>
#include "xmms/xmms.h"
#include "xmms/util.h"
#include "xmms/output.h"

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

	char*               sound_buffer;                  /* temporary buffer used to process data before sending to jack */
	struct timeval      previousTime;                  /* time of last JACK_Callback() write to jack, allows for MS accurate bytes played  */

	jack_port_t*        output_port[MAX_OUTPUT_PORTS]; /* output ports */
	jack_client_t*      client;                        /* pointer to jack client */
	enum status_enum    state;                         /* one of PLAYING, PAUSED, STOPPED, CLOSED, RESET etc*/

	int                 volume[MAX_OUTPUT_PORTS];
	long                sample_rate;                   /* samples(frames) per second */
	unsigned long       num_input_channels;            /* number of input channels(1 is mono, 2 stereo etc..) */
	unsigned long       num_output_channels;           /* number of output channels(1 is mono, 2 stereo etc..) */
	unsigned long       bytes_per_output_frame;        /* (num_output_channels * bits_per_channel) / 8 */
	unsigned long       bytes_per_input_frame;         /* (num_input_channels * bits_per_channel) / 8 */

	unsigned long       written_client_bytes;          /* input bytes we wrote to jack, not necessarily actual bytes we wrote to jack due to channel and other conversion */
	unsigned long       played_client_bytes;           /* input bytes that jack has played */

	unsigned long       client_bytes;                  /* total bytes written by the client via JACK_Write() */

	long                clientBytesInJack;             /* number of INPUT bytes we wrote to jack(not necessary the number of bytes we wrote to jack */
	unsigned long       buffer_size;                   /* number of bytes in the buffer allocated for processing data in JACK_Callback */
} xmms_jack_data_t;


static int
_JACK_OpenDevice(xmms_output_t *output);
static int
_JACK_Open(xmms_output_t *output, unsigned int bytes_per_channel, unsigned long *rate, int channels);


#if CALLBACK_TRACE
#define XMMS_CALLBACK_DBG      XMMS_DBG
#else
#define XMMS_CALLBACK_DBG(...) do {} while(0)
#endif



static gboolean xmms_jack_mixer_set(xmms_output_t *output, gint l, gint r);

/**
 * Alsaplayer function that applies volume changes to a buffer
 * volume is a pointer to an array of integers with the volume\
 * for each channel
 * @param 16bit buffer to apply volume adjustment to
 * @param number of frames to process
 * @param number of channels interleaved in the 16bit buffer
 * @param array of volume values to use, one for each channel
 */
static void
_JACK_volume_effect(void *buffer, int frames, int channels, int* volume)
{
	short *data = (short *)buffer;
	int i, v, j;

	for(i = 0; i < frames; i++)
	{
		for(j = 0; j < channels; j++)
		{
			v = (int) ((*(data) * volume[j]) / 100);
			*(data++) = (v>32767) ? 32767 : ((v<-32768) ? -32768 : v);
		}
	}
}


#define SAMPLE_MAX_16BIT  32767.0f

/**
 * convert from 16 bit to floating point(jack's native format)
 * channels to a buffer that will hold a single channel stream
 * src_skip is in terms of 16bit samples
 * @param array of floating point destination buffers
 * @param source buffer that contains the interleaved 16bit output data
 * @param number of samples to process
 * @param number of destination channels
 * @param number of source channels
 */
static void
_JACK_sample_move_d16_s16 (jack_default_audio_sample_t *dst[MAX_OUTPUT_PORTS], short *src,
			   unsigned long nsamples, int nDstChannels, int nSrcChannels)
{
	int nSrcCount;
	int dstIndex;       /* the current dst output buffer */
	int nDstOffset = 0; /* the current output frame offset */

	if(!nSrcChannels && !nDstChannels)
	{
		xmms_log_fatal("nSrcChannels of %d, nDstChannels of %d, can't have zero channels",
			       nSrcChannels, nDstChannels);
		return; /* in the off chance we have zero channels somewhere */
	}

	while(nsamples--)
	{
		nSrcCount = nSrcChannels; /* keep track of how many output input channels we have */
		dstIndex = 0; /* for each frame start off at the first output channel */

		/* loop until all of our destination channels are filled */
		while(dstIndex < nDstChannels)
		{
			nSrcCount--;

			/* copy the data after converting to floating point */
			*(dst[dstIndex] + nDstOffset) = (*src) / SAMPLE_MAX_16BIT;

			src++; /* advance to the next src channel */

			/* if we ran out of source channels but not destination channels */
			/* then start the src channels back where we were */
			if(!nSrcCount)
			{
				src-=nSrcChannels;
				nSrcCount = nSrcChannels;
			}

			dstIndex++; /* go to the next output buffer */
		}

		/* advance the the position */
		src+=nSrcCount; /* go to the next frame in the src buffer */
		nDstOffset++; /* go to the next frame in the output buffer */
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

	/*  XMMS_CALLBACK_DBG("nframes %ld, sizeof(jack_default_audio_sample_t) == %d", (long)nframes,
	    sizeof(jack_default_audio_sample_t));*/

	this = xmms_output_private_data_get (output);

	if(!this->client)
		xmms_log_fatal("client is closed, this is weird...");

	/*  XMMS_CALLBACK_DBG("num_output_channels = %ld, num_input_channels = %ld",
	    this->num_output_channels, this->num_input_channels);*/

	/* retrieve the buffers for the output ports */
	for(i = 0; i < this->num_output_channels; i++)
		out_buffer[i] = (jack_default_audio_sample_t *) jack_port_get_buffer(this->output_port[i], nframes);

	/* handle playing state */
	if(this->state == PLAYING)
	{
		unsigned long jackFramesAvailable = nframes; /* frames we have left to write to jack */
		unsigned long inputFramesAvailable;          /* frames we have available this loop */
		unsigned long numFramesToWrite;              /* num frames we are writing this loop */

		long written, read;

		written = read = 0;

		XMMS_CALLBACK_DBG("playing... jackFramesAvailable = %ld", jackFramesAvailable);

		/* see if our buffer is large enough for the data we are writing */
		/* ie. Buffer_size < (bytes we already wrote + bytes we are going to write in this loop) */
		/* Note: sound_buffer is always filled with 16-bit data */
		/* so frame * 2 bytes(16 bits) * X output channels */
		if(this->buffer_size < (jackFramesAvailable * sizeof(short) * this->num_output_channels))
		{
			XMMS_DBG("our buffer must have changed size");
			xmms_log_fatal("allocated %ld bytes, need %ld bytes", this->buffer_size,
				       jackFramesAvailable * sizeof(short) * this->num_output_channels);
			return 0;
		}

		XMMS_CALLBACK_DBG("trying to read %ld bytes\n", jackFramesAvailable * sizeof(short) * this->num_input_channels);
		inputFramesAvailable = xmms_output_read(output, this->sound_buffer, jackFramesAvailable * sizeof(short) * this->num_input_channels);
		inputFramesAvailable = inputFramesAvailable / (sizeof(short) * this->num_input_channels);

		XMMS_CALLBACK_DBG("inputFramesAvailable == %ld, jackFramesAvailable == %ld", inputFramesAvailable, jackFramesAvailable);

		numFramesToWrite = min(jackFramesAvailable, inputFramesAvailable); /* write as many bytes as we have space remaining, or as much as we have data to write */

		XMMS_CALLBACK_DBG("nframes == %ld, jackFramesAvailable == %ld,\n\tthis->num_input_channels == %ld, this->num_output_channels == %ld",
				  (long)nframes, jackFramesAvailable, this->num_input_channels, this->num_output_channels);

		/* add on what we wrote to the byte counters */
		written+=(numFramesToWrite * this->bytes_per_output_frame);
		read+=(numFramesToWrite * this->bytes_per_input_frame);

		jackFramesAvailable-=numFramesToWrite; /* take away what was written */

		XMMS_CALLBACK_DBG("jackFramesAvailable == %ld", jackFramesAvailable);

		gettimeofday(&this->previousTime, 0); /* record the current time */
		this->written_client_bytes+=read;
		this->played_client_bytes+=this->clientBytesInJack; /* move forward by the previous bytes we wrote since those must have finished by now */
		this->clientBytesInJack = read; /* record the input bytes we wrote to jack */

		/* Now that we have finished filling the buffer either until it is full or until */
		/* we have run out of application sound data to process, apply volume and output */
		/* the audio to the jack server */

		/* apply volume to the 16bit output sound buffer */
		XMMS_CALLBACK_DBG("applying volume of %d, %d to %ld frames and %ld channels",
				  this->volume[0], this->volume[1],
				  (nframes - jackFramesAvailable), this->num_output_channels);
		_JACK_volume_effect(this->sound_buffer, (nframes - jackFramesAvailable),
				    this->num_output_channels, this->volume);

		/* convert from stereo 16 bit to single channel 32 bit float */
		/* for each output channel */
		/* NOTE: we skip over the number shorts(16 bits) we have output channels as the channel data */
		/* is encoded like chan1,chan2,chan3,chan1,chan2,chan3... */
		XMMS_CALLBACK_DBG("outputting to each channel");
		_JACK_sample_move_d16_s16(out_buffer, (short*)this->sound_buffer,
					  (nframes - jackFramesAvailable), this->num_output_channels,
					  this->num_input_channels);

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
		/*XMMS_CALLBACK_DBG("PAUSED or STOPPED or CLOSED, outputting silence");*/

		gettimeofday(&this->previousTime, 0); /* record the current time */

		/* output silence if nothing is being outputted */
		for(i = 0; i < this->num_output_channels; i++)
			_JACK_sample_silence_dS(out_buffer[i], nframes);

		/* if we were told to reset then zero out some variables */
		/* and transition to STOPPED */
		if(this->state == RESET)
		{
			this->written_client_bytes    = 0;
			this->played_client_bytes     = 0;  /* number of the clients bytes that jack has played */

			this->client_bytes            = 0;  /* bytes that the client wrote to use */
			this->clientBytesInJack       = 0;  /* number of input bytes in jack(not necessary the number of bytes written to jack) */

			this->state = STOPPED; /* transition to STOPPED */
		}
	}

	/*  XMMS_CALLBACK_DBG("callback done");
	*/

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
	/* frames * 2 bytes in 16 bits * X channels of output */
	buffer_required = nframes * sizeof(short) * this->num_output_channels;
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

	XMMS_DBG("trying to reconnect after sleeping for a short while...");

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
	xmms_log_fatal("JACK_Error() %s", desc);
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
	xmms_jack_data_t *this;
	const char** ports;
	int i;
	char client_name[64];
	int failed = 0;

	XMMS_DBG("creating jack client and setting up callbacks");

	this = xmms_output_private_data_get(output);

	/* see if this device is already open */
	if(this->client)
		return ERR_OPENING_JACK;

	/* zero out the buffer pointer and the size of the buffer */
	this->sound_buffer = 0;
	this->buffer_size = 0;

	/* set up an error handler */
	jack_set_error_function(JACK_Error);

	/* try to become a client of the JACK server */
	snprintf(client_name, sizeof(client_name), "xmms_jack_%d_%d", 0, getpid());
	XMMS_DBG("client name '%s'", client_name);
	if ((this->client = jack_client_new(client_name)) == 0)
	{
		/* jack has problems with shutting down clients, so lets */
		/* wait a short while and try once more before we give up */
		if ((this->client = jack_client_new(client_name)) == 0)
		{
			xmms_log_fatal("jack server not running?");
			return ERR_OPENING_JACK;
		}
	}

	/* JACK server to call `JACK_callback()' whenever
	   there is work to be done. */
	jack_set_process_callback(this->client, JACK_callback, output);

	/* setup a buffer size callback */
	jack_set_buffer_size_callback(this->client, JACK_bufsize, output);

	/* tell the JACK server to call `srate()' whenever
	   the sample rate of the system changes. */
	jack_set_sample_rate_callback(this->client, JACK_srate, output);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us. */
	jack_on_shutdown(this->client, JACK_shutdown, output);

	/* display the current sample rate. once the client is activated
	   (see below), you should rely on your own sample rate
	   callback (see above) for this value. */
	this->sample_rate = jack_get_sample_rate(this->client);
	XMMS_DBG("engine sample rate: %lu", this->sample_rate);

	/* create the output ports */
	for(i = 0; i < this->num_output_channels; i++)
	{
		char portname[32];
		sprintf(portname, "out_%d", i);
		XMMS_DBG("port %d is named '%s'", i, portname);
		this->output_port[i] = jack_port_register(this->client, portname,
							  JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	}

	/* set the initial buffer size */
	JACK_bufsize(jack_get_buffer_size(this->client), output);

	/* tell the JACK server that we are ready to roll */
	if(jack_activate(this->client))
	{
		xmms_log_fatal( "cannot activate client");
		return ERR_OPENING_JACK;
	}


	XMMS_DBG("jack_get_ports() passing in NULL/NULL");
	ports = jack_get_ports(this->client, NULL, NULL, JackPortIsInput);


	/* display a trace of the output ports we found */
	for(i = 0; ports[i]; i++)
		XMMS_DBG("ports[%d] = '%s'", i, ports[i]);

	/* see if we have enough ports */
	if(i < this->num_output_channels)
	{
		XMMS_DBG("ERR: jack_get_ports() failed to find ports with jack port flags of 0x%X'", JackPortIsInput);
		return ERR_PORT_NOT_FOUND;
	}

	/* connect the ports. Note: you can't do this before
	   the client is activated (this may change in the future). */
	for(i = 0; i < this->num_output_channels; i++)
	{
		XMMS_DBG("jack_connect() to port '%p'", this->output_port[i]);
		if(jack_connect(this->client, jack_port_name(this->output_port[i]), ports[i]))
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
	xmms_jack_data_t *this;

	this = xmms_output_private_data_get(output);

	output_channels = input_channels = channels;

	_JACK_reset(this); /* flushes all queued buffers, sets status to STOPPED and resets some variables */

	/* this->sample_rate is set by _JACK_OpenDevice() */
	this->num_input_channels     = input_channels;
	this->num_output_channels    = output_channels;
	this->bytes_per_input_frame  = (bytes_per_channel*this->num_input_channels);
	this->bytes_per_output_frame = (bytes_per_channel*this->num_output_channels);

	XMMS_DBG("num_input_channels == %ld", this->num_input_channels);
	XMMS_DBG("num_output_channels == %ld", this->num_output_channels);
	XMMS_DBG("bytes_per_output_frame == %ld", this->bytes_per_output_frame);
	XMMS_DBG("bytes_per_input_frame  == %ld", this->bytes_per_input_frame);

	/* make sure bytes_per_frame is valid and non-zero */
	if(!this->bytes_per_output_frame)
	{
		xmms_log_fatal("bytes_per_output_frame is zero");
		return ERR_BYTES_PER_OUTPUT_FRAME_INVALID;
	}

	/* make sure bytes_per_frame is valid and non-zero */
	if(!this->bytes_per_input_frame)
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
 * Open the device up
 * @param output xmms object
 */
static gboolean
xmms_jack_open(xmms_output_t *output)
{
	xmms_jack_data_t *data;
	XMMS_DBG("xmms_jack_open() called");

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	return TRUE;
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
	data->volume[0] = l;
	data->volume[1] = r;

	return TRUE;
}

/**
 * Get the current volume setting
 * @param output xmms object
 * @param pointer to left volume level
 * @param pointer to right volume level
 **/
static gboolean
xmms_jack_mixer_get(xmms_output_t *output, gint *l, gint *r)
{
	xmms_jack_data_t *data;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	*l = data->volume[0];
	*r = data->volume[1];

	XMMS_DBG("l(%d), r(%d)", *l, *r);

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
xmms_jack_samplerate_set (xmms_output_t *output, guint rate)
{
	xmms_jack_data_t *data;

	XMMS_DBG ("xmms_jack_samplerate_set");
	XMMS_DBG ("trying to change rate to: %d but we can't change jacks rate", rate);

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);	

	return data->rate;
}

/**
 * Close the device
 * @param output xmms object
 */
static void
xmms_jack_close(xmms_output_t *output)
{
	xmms_jack_data_t *this;

	g_return_if_fail (output);
	this = xmms_output_private_data_get (output);
	g_return_if_fail (this);

	XMMS_DBG("\n");

	_JACK_CloseDevice(output);
	_JACK_reset(this);
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
	long outputFrequency = 0;
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
	data->volume[0] = 80; /* set volume to 80% */
	data->volume[1] = 80;

	XMMS_DBG("jack initialized!!");

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
xmms_jack_status (xmms_output_t *output, xmms_output_status_t status)
{
	xmms_jack_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG ("changed status! %d", status);
	if(status == XMMS_OUTPUT_STATUS_PLAY) {
		data->state = PLAYING;
	} else {
		data->state = STOPPED;
	}
}

/**
 * Get plugin information
 */
xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "jack",
				  "jack Output" XMMS_VERSION,
				  "Jack audio server output plugin");

	xmms_plugin_info_add (plugin, "URL", "http://xmms-jack.sf.net");
	xmms_plugin_info_add (plugin, "Author", "Chris Morgan");
	xmms_plugin_info_add (plugin, "E-Mail", "cmorgan@alum.wpi.edu");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, 
				xmms_jack_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, 
				xmms_jack_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, 
				xmms_jack_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, 
				xmms_jack_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, 
				xmms_jack_samplerate_set);
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

