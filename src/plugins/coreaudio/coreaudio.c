/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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


/** @file CoreAudio for MacOSX */


#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/config.h"
#include "xmms/signal_xmms.h"

#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <stdio.h>

#include <glib.h>

#include <CoreAudio/CoreAudio.h>

/*
 * Type definitions
 */

typedef struct xmms_ca_data_St {
	AudioDeviceID outputdevice;
	xmms_ringbuf_t *buffer;
	GMutex *mtx;
	guint rate;
} xmms_ca_data_t;

/*
 * Function prototypes
 */

static void xmms_ca_status (xmms_output_t *output, xmms_output_status_t status);
static gboolean xmms_ca_open (xmms_output_t *output);
static gboolean xmms_ca_new (xmms_output_t *output);
static void xmms_ca_destroy (xmms_output_t *output);
static void xmms_ca_close (xmms_output_t *output);
static void xmms_ca_flush (xmms_output_t *output);
static guint xmms_ca_samplerate_set (xmms_output_t *output, guint rate);
static guint xmms_ca_buffersize_get (xmms_output_t *output);

static OSStatus
xmms_ca_write_cb (AudioDeviceID inDevice,
		  const AudioTimeStamp *inNow,
		  const AudioBufferList *inInputData,
		  const AudioTimeStamp *inInputTime,
		  AudioBufferList *outOutputData,
		  const AudioTimeStamp *inOutputTime,
		  void *inClientData);


/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "coreaudio",
			"CoreAudio Output " XMMS_VERSION,
			"Darwin CoreAudio Output Support");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "INFO", "http://www.apple.com/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_STATUS, xmms_ca_status);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, xmms_ca_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_ca_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_ca_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_ca_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, xmms_ca_samplerate_set);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET, xmms_ca_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, xmms_ca_flush);

	return plugin;
}

/*
 * Member functions
 */

static void
xmms_ca_status (xmms_output_t *output, xmms_output_status_t status)
{
	xmms_ca_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG ("changed status! %d", status);
	if (status == XMMS_OUTPUT_STATUS_PLAY) {
		AudioDeviceStart (data->outputdevice, xmms_ca_write_cb);
	} else {
		AudioDeviceStop (data->outputdevice, xmms_ca_write_cb);
	}
}

static guint
xmms_ca_buffersize_get (xmms_output_t *output)
{
	guint ret;
	xmms_ca_data_t *data;
	UInt32 f;
	UInt32 size;
	OSStatus res;

	g_return_val_if_fail (output, 0);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	g_mutex_lock (data->mtx);
	ret = xmms_ringbuf_bytes_used (data->buffer) / 2;

	size = sizeof (UInt32);

	res = AudioDeviceGetProperty (data->outputdevice, 1, 0, 
				      kAudioDevicePropertyLatency,
				      &size, &f);

	ret += (f * 4);

	g_mutex_unlock (data->mtx);

	return ret;
}

static void
xmms_ca_flush (xmms_output_t *output)
{
	XMMS_DBG ("Xmms wants us to flush!");
}


/* CoreAudio Callback */
static OSStatus
xmms_ca_write_cb (AudioDeviceID inDevice,
		  const AudioTimeStamp *inNow,
		  const AudioBufferList *inInputData,
		  const AudioTimeStamp *inInputTime,
		  AudioBufferList *outOutputData,
		  const AudioTimeStamp *inOutputTime,
		  void *inClientData)
{

	xmms_ca_data_t *data;
	xmms_output_t *output;
	int b;

	output = (xmms_output_t *)inClientData;
	g_return_val_if_fail (output, kAudioHardwareUnspecifiedError);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, kAudioHardwareUnspecifiedError);

	for (b = 0; b < outOutputData->mNumberBuffers; ++b) {
		gint m;
		gint i;
		gint ret;
		gint16 *buffer;

		m = outOutputData->mBuffers[b].mDataByteSize;

		buffer = g_new0 (gint16, m/2);

		ret = xmms_output_read (output, 
					(gchar *)buffer, 
					m/2);

		for (i = 0; i < (ret/2) ; i++) {
			((gfloat*)outOutputData->mBuffers[0].mData)[i] = ((gfloat) buffer[i] + 32768.0) / (32768.0*2);
		}

		g_free (buffer);


		/** @todo fix so coreaudio knows if we
		    don't fill the buffer ? */

	}

	return kAudioHardwareNoError;

}

static gboolean
xmms_ca_open (xmms_output_t *output)
{
	xmms_ca_data_t *data;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);

	//AudioDeviceStart (data->outputdevice, xmms_ca_write_cb);
	
	XMMS_DBG ("xmms_ca_open (%p)", output);

	return TRUE;
}

static gboolean
xmms_ca_new (xmms_output_t *output)
{
	xmms_ca_data_t *data;
	OSStatus res;
	UInt32 size;
	
	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_ca_data_t, 1);
	data->mtx = g_mutex_new ();
	data->buffer = xmms_ringbuf_new (4096 * 20);

	size = sizeof (data->outputdevice);

	/* Maybe the kAudio... could be configurable ? */
	res = AudioHardwareGetProperty (kAudioHardwarePropertyDefaultOutputDevice,
					&size, &data->outputdevice);

	if (res) {
		xmms_log_error ("Error %d from CoreAudio", (int) res);
		return FALSE;
	}

	/*size = sizeof (guint32);
	bsize = 4096;
	res = AudioDeviceSetProperty (data->outputdevice, 0, 0, 0,
					kAudioDevicePropertyBufferSize,
					size,
					&bsize);

	if (res) {
		xmms_log_error ("Setbuffersize failed");
		return FALSE;
	}*/

	res = AudioDeviceAddIOProc (data->outputdevice, xmms_ca_write_cb, (void *) output);
	
	xmms_output_private_data_set (output, data);

	XMMS_DBG ("CoreAudio initilized!");
	
	return TRUE;
}

static void
xmms_ca_destroy (xmms_output_t *output)
{
	xmms_ca_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	g_mutex_free (data->mtx);
	xmms_ringbuf_destroy (data->buffer);

	/** @todo
	 *  Do we have to take care of data->outputdevice
	 *  here, too?
	 */

	g_free (data);
}

static guint
xmms_ca_samplerate_set (xmms_output_t *output, guint rate)
{
	xmms_ca_data_t *data;
	OSStatus res;
	struct AudioStreamBasicDescription prop;
	UInt32 size;

	g_return_val_if_fail (output, 0);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	size = sizeof (struct AudioStreamBasicDescription);

	memset (&prop, 0, sizeof (prop));

	g_mutex_lock (data->mtx);

	data->rate = rate;

	res = AudioDeviceGetProperty (data->outputdevice, 1, 0, 
				      kAudioDevicePropertyStreamFormat,
				      &size, &prop);

	if (res) {
		xmms_log_error ("GetProp failed!");
		return 0;
	}

	/*prop.mSampleRate = rate;
	prop.mFormatID = kAudioFormatLinearPCM;
	prop.mFormatFlags |= ~kAudioFormatFlagIsFloat;
	prop.mBitsPerChannel = 32;
	prop.mBytesPerFrame = 8;
	prop.mChannelsPerFrame = 2;

	res = AudioDeviceSetProperty (data->outputdevice, 0, 0, 0,
				      kAudioDevicePropertyStreamFormat,
				      size, &prop);
	if (res) {
		xmms_log_error ("Error %d", (int) res);
		exit (-1);
	}*/

	g_mutex_unlock (data->mtx);

	return prop.mSampleRate;
}

static void
xmms_ca_close (xmms_output_t *output)
{
	xmms_ca_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	AudioDeviceStop (data->outputdevice, xmms_ca_write_cb);
}

/*
static void
xmms_ca_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_ca_data_t *data;
	gfloat *buf;
	gint i;
	gint16 *buffer2 = (gint16 *) buffer;

	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	data = xmms_output_private_data_get (output);

	buf = g_new0 (gfloat, (len/2));

	for (i = 0; i < (len/2) ; i++) {
		buf[i] = ((gfloat) buffer2[i] + 32768.0) / (32768.0*2);
	}
	
	g_mutex_lock (data->mtx);
	xmms_ringbuf_wait_free (data->buffer, len*2, data->mtx);
	xmms_ringbuf_write (data->buffer, buf, len*2);
	g_mutex_unlock (data->mtx);

	g_free (buf);

}
*/
