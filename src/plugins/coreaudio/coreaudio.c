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

static void xmms_ca_status (xmms_output_t *output, xmms_output_status_t status);
static gboolean xmms_ca_open (xmms_output_t *output);
static gboolean xmms_ca_new (xmms_output_t *output);
static void xmms_ca_destroy (xmms_output_t *output);
static void xmms_ca_close (xmms_output_t *output);
static void xmms_ca_flush (xmms_output_t *output);
static guint xmms_ca_buffersize_get (xmms_output_t *output);
static void xmms_ca_mixer_config_changed (xmms_object_t *object, gconstpointer data, gpointer userdata);
void xmms_ca_mixer_set (xmms_output_t *output, guint left, guint right);
static gboolean xmms_ca_format_set (xmms_output_t *output, xmms_audio_format_t *format);

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
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET, xmms_ca_buffersize_get);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, xmms_ca_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FORMAT_SET, xmms_ca_format_set);
	
	xmms_plugin_config_value_register (plugin, "volume", "70/70", NULL, NULL);

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
	xmms_plugin_t *plugin;
	xmms_config_value_t *volume;
	

	OSStatus res;
	ComponentDescription desc;
	AURenderCallbackStruct input;
	Component comp;
	AudioDeviceID device = NULL;
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
	
	if (device != NULL) {
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
	
	plugin = xmms_output_plugin_get (output);
	volume = xmms_plugin_config_lookup (plugin, "volume");
	xmms_config_value_callback_set (volume,
									xmms_ca_mixer_config_changed,
									(gpointer) output);

	XMMS_DBG ("CoreAudio initialized!");

	data->running = FALSE;
	xmms_output_private_data_set (output, data);

	return TRUE;
}

static void
xmms_ca_destroy (xmms_output_t *output)
{
	xmms_ca_data_t *data;
	xmms_plugin_t *plugin;
	xmms_config_value_t *volume;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	plugin = xmms_output_plugin_get (output);
	volume = xmms_plugin_config_lookup (plugin, "volume");
	xmms_config_value_callback_remove (volume,
	                                   xmms_ca_mixer_config_changed);


	AudioUnitUninitialize (data->au);
	CloseComponent (data->au);

	g_free (data);
}

static void xmms_ca_mixer_config_changed (xmms_object_t *object, 
										  gconstpointer data,
										  gpointer userdata)
{
  	xmms_ca_data_t *ca_data;
  	guint res, left, right;
	
	g_return_if_fail (data);
	g_return_if_fail (userdata);
	ca_data = xmms_output_private_data_get (userdata);
	g_return_if_fail (ca_data);

	res = sscanf (data, "%u/%u", &left, &right);

	if (res == 0) {
	  	xmms_log_error ("Unable to change volume");
	}
	else {
		xmms_ca_mixer_set (userdata, left, right);
	}
}

void xmms_ca_mixer_set (xmms_output_t *output, guint left, 
						guint right)
{
  	Float32 volume;
  	xmms_ca_data_t *data;
 	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);
	
	volume = (Float32)(left/255.0);
	AudioUnitSetParameter(data->au, 
						  kHALOutputParam_Volume, 
						  kAudioUnitScope_Global, 
						  0, volume, 0);
}

static void
xmms_ca_close (xmms_output_t *output)
{
	xmms_ca_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	AudioUnitReset (data->au, kAudioUnitScope_Input, 0);
}

static gboolean
xmms_ca_format_set (xmms_output_t *output, xmms_audio_format_t *format)
{
	return TRUE;
}
