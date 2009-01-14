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


/** @file CoreAudio for MacOSX */


#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#undef DEBUG
#include <CoreServices/CoreServices.h>

#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <stdio.h>

#include <glib.h>

#include <AudioUnit/AudioUnit.h>

/*
 * Type definitions
 */

typedef struct xmms_ca_data_St {
	AudioUnit au;
	gboolean running;
} xmms_ca_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_ca_plugin_setup (xmms_output_plugin_t *plugin);
static gboolean xmms_ca_status (xmms_output_t *output, xmms_playback_status_t status);
static gboolean xmms_ca_new (xmms_output_t *output);
static void xmms_ca_destroy (xmms_output_t *output);
static void xmms_ca_flush (xmms_output_t *output);
static guint xmms_ca_buffersize_get (xmms_output_t *output);
void xmms_ca_mixer_set (xmms_output_t *output, guint left, guint right);
static gboolean xmms_ca_volume_set (xmms_output_t *output, const gchar
                                    *channel, guint volume);

static gboolean xmms_ca_volume_get (xmms_output_t *output, const gchar
                                    **names, guint *values, guint
                                    *num_channels);
static gboolean xmms_ca_format_set (xmms_output_t *output,
                                    const xmms_stream_type_t *stype);
/*
 * Plugin header
 */


XMMS_OUTPUT_PLUGIN ("coreaudio", "CoreAudio Output", XMMS_VERSION,
                    "MacOSX CoreAudio output plugin",
                    xmms_ca_plugin_setup);


static gboolean
xmms_ca_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);
	methods.new = xmms_ca_new;
	methods.destroy = xmms_ca_destroy;

	methods.flush = xmms_ca_flush;
	methods.format_set = xmms_ca_format_set;

	methods.volume_get = xmms_ca_volume_get;
	methods.volume_set = xmms_ca_volume_set;

	methods.status = xmms_ca_status;

	methods.latency_get = xmms_ca_buffersize_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	return TRUE;
}

/*
 * Member functions
 */

static gboolean
xmms_ca_status (xmms_output_t *output, xmms_playback_status_t status)
{
	xmms_ca_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (output, FALSE);

	XMMS_DBG ("changed status! %d", status);
	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		if (!data->running) {
			XMMS_DBG ("start polling!");
			AudioOutputUnitStart (data->au);
			data->running = TRUE;
		}
	} else {
		if (data->running) {
			XMMS_DBG ("stop polling!");
			AudioOutputUnitStop (data->au);
			data->running = FALSE;
		}

	}

	return TRUE;
}

static guint
xmms_ca_buffersize_get (xmms_output_t *output)
{
	return 0;
}

static void
xmms_ca_flush (xmms_output_t *output)
{
	xmms_ca_data_t *data;
	OSStatus res;

	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	res = AudioUnitReset (data->au, kAudioUnitScope_Global, 0);
	if (res) {
		xmms_log_error ("Reset failed!");
	}
}

OSStatus
xmms_ca_render_cb (void *inRefCon,
                   AudioUnitRenderActionFlags *ioActionFlags,
                   const AudioTimeStamp *inTimeStamp,
                   UInt32 inBusNumber,
                   UInt32 inNumberFrames,
                   AudioBufferList *ioData)
{
	xmms_ca_data_t *data;
	gint b;

	xmms_output_t *output = (xmms_output_t *)inRefCon;
	g_return_val_if_fail (output, kAudioHardwareUnspecifiedError);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, kAudioHardwareUnspecifiedError);

	for (b = 0; b < ioData->mNumberBuffers; ++ b) {
		gint size;
		gint ret;

		size = ioData->mBuffers[b].mDataByteSize;

		ret = xmms_output_read (output, (gchar *)ioData->mBuffers[b].mData, size);
		if (ret == -1)
			ret = 0;

		if (ret < size) {
			memset (ioData->mBuffers[b].mData+ret, 0, size - ret);
		}
	}

	return noErr;
}

static gboolean
xmms_ca_format_set (xmms_output_t *output,
                    const xmms_stream_type_t *stype)
{
	return TRUE;
}

static gboolean
xmms_ca_new (xmms_output_t *output)
{
	xmms_ca_data_t *data;

	OSStatus res;
	ComponentDescription desc;
	AURenderCallbackStruct input;
	AudioStreamBasicDescription format;
	Component comp;
	AudioDeviceID device = 0;
	UInt32 size = sizeof (device);

	g_return_val_if_fail (output, FALSE);

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent (NULL, &desc);
	if (!comp) {
		xmms_log_error ("Couldn't find that component in my list!");
		return FALSE;
	}

	data = g_new0 (xmms_ca_data_t, 1);

	res = OpenAComponent (comp, &data->au);
	if (comp == NULL) {
		xmms_log_error ("Opening component failed!");
		g_free (data);
		return FALSE;
	}

	input.inputProc = xmms_ca_render_cb;
	input.inputProcRefCon = (void*)output;

	res = AudioUnitSetProperty (data->au, kAudioUnitProperty_SetRenderCallback,
	                            kAudioUnitScope_Input, 0,
	                            &input, sizeof (input));

	if (res) {
		xmms_log_error ("Set Callback failed!");
		g_free (data);
		return FALSE;
	}

	format.mSampleRate = 44100.0;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
	format.mBytesPerPacket = 4;
	format.mFramesPerPacket = 1;
	format.mBytesPerFrame = 4;
	format.mChannelsPerFrame = 2;
	format.mBitsPerChannel = 16;

	res = AudioUnitSetProperty (data->au, kAudioUnitProperty_StreamFormat,
	                            kAudioUnitScope_Input, 0, &format,
	                            sizeof (AudioStreamBasicDescription));
	if (res) {
		xmms_log_error ("Failed to set format");
		return FALSE;
	}

	res = AudioUnitInitialize (data->au);
	if (res) {
		xmms_log_error ("Audio Unit wouldn't initialize!");
		g_free (data);
		return FALSE;
	}

	XMMS_DBG ("CoreAudio initialized!");

	res = AudioUnitGetProperty (data->au,
	                            kAudioOutputUnitProperty_CurrentDevice,
	                            kAudioUnitScope_Global,
	                            0,
	                            &device,
	                            &size);

	if (res) {
		xmms_log_error ("getprop failed!");
		g_free (data);
		return FALSE;
	}

	/* static for now */
	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_S16, 2, 44100);

	data->running = FALSE;
	xmms_output_private_data_set (output, data);

	return TRUE;
}

static void
xmms_ca_destroy (xmms_output_t *output)
{
	xmms_ca_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	AudioUnitUninitialize (data->au);
	CloseComponent (data->au);

	g_free (data);
}


static gboolean
xmms_ca_volume_set (xmms_output_t *output,
                    const gchar *channel,
                    guint volume)
{
	Float32 v;
	xmms_ca_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!g_ascii_strcasecmp (channel, "master") == 0) {
		return FALSE;
	}

	v = (Float32)(volume/100.0);

	AudioUnitSetParameter (data->au,
	                       kHALOutputParam_Volume,
	                       kAudioUnitScope_Global,
	                       0, v, 0);
	return TRUE;
}


static gboolean
xmms_ca_volume_get (xmms_output_t *output,
                    const gchar **names, guint *values,
                    guint *num_channels)
{
	Float32 v;
	xmms_ca_data_t *data;

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	if (!*num_channels) {
		*num_channels = 1;
		return TRUE;
	}

	g_return_val_if_fail (*num_channels == 1, FALSE);
	g_return_val_if_fail (names, FALSE);
	g_return_val_if_fail (values, FALSE);

	AudioUnitGetParameter (data->au,
	                       kHALOutputParam_Volume,
	                       kAudioUnitScope_Global,
	                       0, &v);

	values[0] = (guint)(v * 100);
	names[0] = "master";

	return TRUE;

}
