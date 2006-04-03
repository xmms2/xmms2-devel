/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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


#include "xmms/xmms_defs.h"
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
	AudioStreamBasicDescription sF;
	gboolean running;
} xmms_ca_data_t;

/*
 * Function prototypes
 */

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
/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, 
	                          XMMS_OUTPUT_PLUGIN_API_VERSION,
	                          "coreaudio",
	                          "CoreAudio Output",
	                          XMMS_VERSION,
	                          "Darwin CoreAudio Output Support");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "INFO", "http://www.apple.com/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_STATUS, xmms_ca_status);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_ca_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_ca_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET, xmms_ca_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, xmms_ca_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_VOLUME_GET, xmms_ca_volume_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_VOLUME_SET, xmms_ca_volume_set);

	return plugin;
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
			AudioOutputUnitStart (data->au);
			data->running = TRUE;
		}
	} else {
		if (data->running) {
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
	XMMS_DBG ("Xmms wants us to flush!");
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

		if (ret < size) 
			memset (ioData->mBuffers[b].mData+ret, 0, size - ret);
	}

	return noErr;
}

static gboolean
xmms_ca_new (xmms_output_t *output)
{
	xmms_ca_data_t *data;

	OSStatus res;
	ComponentDescription desc;
	AURenderCallbackStruct input;
	Component comp;
	AudioDeviceID device = 0;
	UInt32 size = sizeof(device);
	
	g_return_val_if_fail (output, FALSE);

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent (NULL, &desc);
	if (!comp) {
		return FALSE;
	}

	data = g_new0 (xmms_ca_data_t, 1);

	res = OpenAComponent (comp, &data->au);
	if (comp == NULL) {
		g_free (data);
		return FALSE;
	}


	input.inputProc = xmms_ca_render_cb;
	input.inputProcRefCon = (void*)output;

	res = AudioUnitSetProperty (data->au, kAudioUnitProperty_SetRenderCallback,
				    kAudioUnitScope_Input, 0,
				    &input, sizeof (input));

	if (res) {
		XMMS_DBG ("Set Callback failed!");
		g_free (data);
		return FALSE;
	}


	data->sF.mSampleRate = 44100.0;
	data->sF.mFormatID = kAudioFormatLinearPCM;
	data->sF.mFormatFlags = 0;
	data->sF.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
	data->sF.mBytesPerPacket = 4;
	data->sF.mFramesPerPacket = 1;
	data->sF.mBytesPerFrame = 4;
	data->sF.mChannelsPerFrame = 2;
	data->sF.mBitsPerChannel = 16;

	res = AudioUnitSetProperty (data->au, kAudioUnitProperty_StreamFormat,
				    kAudioUnitScope_Input, 0, &data->sF,
				    sizeof(AudioStreamBasicDescription));

	if (res) {
		g_free (data);
		return FALSE;
	}

	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_S16, 2, 44100.0);

	res = AudioUnitInitialize (data->au);
	if (res) {
		g_free (data);
		return FALSE;
	}


	res = AudioUnitGetProperty( data->au,
				    kAudioOutputUnitProperty_CurrentDevice,
				    kAudioUnitScope_Global,
				    0,
				    &device,
				    &size);

	if (res) {
		XMMS_DBG ("getprop failed!");
		g_free (data);
		return FALSE;
	}
	
	if (device != 0) {
		AudioTimeStamp ts;
		ts.mFlags = 0;
		UInt32 bufferSize = 4096;
		res = AudioDeviceSetProperty( device,
					      &ts, 
					      0,
					      0,
					      kAudioDevicePropertyBufferFrameSize,
					      sizeof(UInt32),
					      &bufferSize);
		if (res) {
			XMMS_DBG ("Set prop failed!");
			g_free (data);
			return FALSE;
		}
	}
	
	XMMS_DBG ("CoreAudio initialized!");

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

	if (!g_strcasecmp (channel, "master") == 0) {
		return FALSE;
	}

	v = (Float32)(volume/100.0);
	
	AudioUnitSetParameter(data->au, 
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

	AudioUnitGetParameter(data->au, 
						  kHALOutputParam_Volume, 
						  kAudioUnitScope_Global, 
						  0, &v);

	values[0] = (guint)(v * 100);
	names[0] = "master";

	return TRUE;

}
