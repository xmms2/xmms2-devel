/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#include "xmms/xmms_outputplugin.h"
#include "xmms/xmms_log.h"

#include <windows.h>
#include <mmsystem.h>
#include <glib.h>



/*
 * Defines
 */

#define BLOCK_SIZE 16384
#define NUM_BLOCKS 6



/*
 * Type definitions
 */

typedef struct xmms_waveout_data_St {
	HWAVEOUT waveout;
	WAVEHDR *blocks;
	gint current_block;
	gint free_blocks;
	GMutex *mutex;
	GCond *cond;
} xmms_waveout_data_t;



/*
 * Function prototypes
 */

static gboolean xmms_waveout_plugin_setup (xmms_output_plugin_t *plugin);

static gboolean xmms_waveout_new (xmms_output_t *output);
static void xmms_waveout_destroy (xmms_output_t *output);

static gboolean xmms_waveout_open (xmms_output_t *output);
static void xmms_waveout_close (xmms_output_t *output);

static void xmms_waveout_flush (xmms_output_t *output);
static gboolean xmms_waveout_format_set (xmms_output_t *output,
                                         const xmms_stream_type_t *format);

static void xmms_waveout_write (xmms_output_t *output, gpointer buffer,
                                gint len, xmms_error_t *err);

static gboolean xmms_waveout_volume_set (xmms_output_t *output,
                                         const gchar *channel_name,
                                         guint volume);
static gboolean xmms_waveout_volume_get (xmms_output_t *output,
                                         const gchar **names, guint *values,
                                         guint *num_channels);

static guint xmms_waveout_buffer_bytes_get (xmms_output_t *output);

static void CALLBACK on_block_done (HWAVEOUT hWaveOut, UINT uMsg,
                                    DWORD dwInstance,
                                    DWORD dwParam1, DWORD dwParam2);



/*
 * Plugin header
 */

XMMS_OUTPUT_PLUGIN ("waveout", "WaveOut Output", XMMS_VERSION,
                    "WaveOut Output plugin",
                    xmms_waveout_plugin_setup);



static gboolean
xmms_waveout_plugin_setup (xmms_output_plugin_t *plugin)
{
	xmms_output_methods_t methods;

	XMMS_OUTPUT_METHODS_INIT (methods);

	methods.new = xmms_waveout_new;
	methods.destroy = xmms_waveout_destroy;

	methods.open = xmms_waveout_open;
	methods.close = xmms_waveout_close;

	methods.flush = xmms_waveout_flush;
	methods.format_set = xmms_waveout_format_set;

	methods.write = xmms_waveout_write;

	methods.volume_get = xmms_waveout_volume_get;
	methods.volume_set = xmms_waveout_volume_set;

	methods.latency_get = xmms_waveout_buffer_bytes_get;

	xmms_output_plugin_methods_set (plugin, &methods);

	return TRUE;
}



/*
 * Member functions
 */

static gboolean
xmms_waveout_new (xmms_output_t *output)
{
	xmms_waveout_data_t *data;
	guchar *chunks;
	gint i;

	g_return_val_if_fail (output, FALSE);

	data = g_new0 (xmms_waveout_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	chunks = g_malloc ((sizeof (WAVEHDR) + BLOCK_SIZE) * NUM_BLOCKS);
	if (!chunks) {
		return FALSE;
	}

	data->blocks = (WAVEHDR *) chunks;
	chunks += sizeof (WAVEHDR) * NUM_BLOCKS;

	for (i = 0; i < NUM_BLOCKS; i++) {
		data->blocks[i].lpData = chunks;
		data->blocks[i].dwBufferLength = BLOCK_SIZE;
		data->blocks[i].dwFlags = WHDR_DONE;
		data->blocks[i].dwUser = 0;

		chunks += BLOCK_SIZE;
	}

	data->free_blocks = NUM_BLOCKS;
	data->current_block = 0;

	data->mutex = g_mutex_new ();
	data->cond = g_cond_new ();

	xmms_output_private_data_set (output, data);

	xmms_output_format_add (output, XMMS_SAMPLE_FORMAT_S16, 2, 44100);

	return TRUE;
}


static void
xmms_waveout_destroy (xmms_output_t *output)
{
	xmms_waveout_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->blocks) {
		g_free (data->blocks);
	}

	if (data->mutex) {
		g_mutex_free (data->mutex);
	}

	if (data->cond) {
		g_cond_free (data->cond);
	}

	g_free (data);
}


static gboolean
xmms_waveout_open (xmms_output_t *output)
{
	xmms_waveout_data_t *data;
	WAVEFORMATEX fmt;
	MMRESULT res;
	gchar buf[128];

	g_return_val_if_fail (output, FALSE);
	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	fmt.nSamplesPerSec = 44100;
	fmt.wBitsPerSample = 16;
	fmt.nChannels = 2;
	fmt.cbSize = 0;
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nBlockAlign = fmt.nChannels * (fmt.wBitsPerSample / 8);
	fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

	res = waveOutOpen (&data->waveout, WAVE_MAPPER, &fmt,
	                   (DWORD_PTR) on_block_done, (DWORD_PTR) data,
	                   CALLBACK_FUNCTION);

	if (res != MMSYSERR_NOERROR) {
		waveOutGetErrorText (res, buf, sizeof (buf));
		xmms_log_error ("Unable to open audio device. %s", buf);
		return FALSE;
	}

	return TRUE;
}


static void
xmms_waveout_close (xmms_output_t *output)
{
	xmms_waveout_data_t *data;
	MMRESULT res;
	gchar buf[128];
	gint i;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (!data->waveout) {
		return;
	}

	waveOutReset (data->waveout);

	for (i = 0; i < NUM_BLOCKS; i++) {
		if (data->blocks[i].dwFlags & WHDR_PREPARED) {
			res = waveOutUnprepareHeader (data->waveout, &data->blocks[i],
			                              sizeof (WAVEHDR));
			if (res != MMSYSERR_NOERROR) {
				waveOutGetErrorText (res, buf, sizeof (buf));
				xmms_log_error ("Could not unprepare header. %s", buf);
			}
		}
	}

	res = waveOutClose (data->waveout);
	if (res != MMSYSERR_NOERROR) {
		waveOutGetErrorText (res, buf, sizeof (buf));
		xmms_log_error ("Unable to close the audio device. %s", buf);
	}

	data->waveout = NULL;
}


static void
xmms_waveout_flush (xmms_output_t *output)
{
	xmms_waveout_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	if (data->waveout) {
		waveOutReset (data->waveout);
	}
}


static gboolean
xmms_waveout_format_set (xmms_output_t *output,
                         const xmms_stream_type_t *format)
{
	/*
	 * Format is locked to 44.1kHz, 16bit stereo.
	 * WaveMapper handles the conversion for now.
	 */

	return TRUE;
}


static void
xmms_waveout_write (xmms_output_t *output, gpointer buffer, gint len,
                    xmms_error_t *err)
{
	xmms_waveout_data_t *data;

	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	g_return_if_fail (output);
	data = xmms_output_private_data_get (output);
	g_return_if_fail (data);

	g_return_if_fail (data->waveout);

	while (len > 0) {
		MMRESULT res;
		gchar buf[128];
		gint remain;

		/* wait for free blocks */
		g_mutex_lock (data->mutex);
		if (!data->free_blocks) {
			g_cond_wait (data->cond, data->mutex);
		}
		g_mutex_unlock (data->mutex);

		WAVEHDR *block = &data->blocks[data->current_block];
		if (block->dwFlags & WHDR_PREPARED) {
			res = waveOutUnprepareHeader (data->waveout, block,
			                              sizeof (WAVEHDR));
			if (res != MMSYSERR_NOERROR) {
				waveOutGetErrorText (res, buf, sizeof (buf));
				xmms_log_error ("Could not unprepare header. %s", buf);
			}
		}

		if (len < (BLOCK_SIZE - block->dwUser)) {
			memcpy (block->lpData + block->dwUser, buffer, len);
			block->dwUser += len;
			break;
		}

		remain = BLOCK_SIZE - block->dwUser;
		memcpy (block->lpData + block->dwUser, buffer, remain);

		len -= remain;
		buffer += remain;

		res = waveOutPrepareHeader (data->waveout, block, sizeof (WAVEHDR));
		if (res != MMSYSERR_NOERROR) {
			waveOutGetErrorText (res, buf, sizeof (buf));
			xmms_log_error ("Could not prepare header. %s", buf);
			break;
		}

		res = waveOutWrite (data->waveout, block, sizeof (WAVEHDR));
		if (res != MMSYSERR_NOERROR) {
			gchar buf[128];
			waveOutGetErrorText (res, buf, sizeof (buf));
			xmms_log_error ("Could not write data to audio device. %s", buf);
			break;
		}

		g_mutex_lock (data->mutex);
		data->free_blocks--;
		g_mutex_unlock (data->mutex);

		data->current_block = (data->current_block + 1) % NUM_BLOCKS;
		data->blocks[data->current_block].dwUser = 0;
	}
}

/**
 * Get mixer settings.
 *
 * @param output The output struct containing waveout data.
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_waveout_volume_get (xmms_output_t *output, const gchar **names,
                         guint *values, guint *num_channels)
{
	xmms_waveout_data_t *data;
	DWORD tmp = 0;
	guint32 left, right;

	g_return_val_if_fail (output, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	g_return_val_if_fail (num_channels, FALSE);

	if (!*num_channels) {
		*num_channels = 2;
		return TRUE;
	}

	g_return_val_if_fail (*num_channels == 2, FALSE);
	g_return_val_if_fail (names, FALSE);
	g_return_val_if_fail (values, FALSE);

	waveOutGetVolume (data->waveout, &tmp);

	right = (tmp & 0xffff0000) >> 16;
	left = (tmp & 0x0000ffff);

	names[0] = "right";
	names[1] = "left";

	/* scale from 0x0000..0xffff to 0..100 */
	right = (right * 100.0 / 0xffff) + 0.5;
	left = (left * 100.0 / 0xffff) + 0.5;

	values[0] = right;
	values[1] = left;

	return TRUE;
}

/**
 * Change mixer settings.
 *
 * @param output The output struct containing waveout data.
 * @param channel_name The name of the channel to set (e.g. "left").
 * @param volume The volume to set the channel to.
 * @return TRUE on success, FALSE on error.
 */
static gboolean
xmms_waveout_volume_set (xmms_output_t *output,
                         const gchar *channel_name, guint volume)
{
	xmms_waveout_data_t *data;
	DWORD tmp = 0;
	guint32 left, right;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (channel_name, FALSE);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, FALSE);

	g_return_val_if_fail (volume <= 100, FALSE);

	waveOutGetVolume (data->waveout, &tmp);

	right = (tmp & 0xffff0000) >> 16;
	left = (tmp & 0x0000ffff);

	/* scale from 0..100 to 0x0000..0xffff */
	volume = (volume * 0xffff / 100.0) + 0.5;

	/* update the channel volumes */
	if (!strcmp (channel_name, "right")) {
		right = volume;
	} else if (!strcmp (channel_name, "left")) {
		left = volume;
	} else {
		return FALSE;
	}

	/* and write it back again */
	volume = (right << 16) | left;

	waveOutSetVolume (data->waveout, volume);

	return TRUE;
}

static guint
xmms_waveout_buffer_bytes_get (xmms_output_t *output)
{
	xmms_waveout_data_t *data;
	guint ret;

	g_return_val_if_fail (output, 0);

	data = xmms_output_private_data_get (output);
	g_return_val_if_fail (data, 0);

	g_mutex_lock (data->mutex);
	ret = (NUM_BLOCKS - data->free_blocks) * (BLOCK_SIZE);
	g_mutex_unlock (data->mutex);

	return ret;
}


static void CALLBACK
on_block_done (HWAVEOUT hWaveOut,UINT uMsg,DWORD dwInstance,
               DWORD dwParam1,DWORD dwParam2)
{
	xmms_waveout_data_t *data = (xmms_waveout_data_t *) dwInstance;

	if (uMsg != WOM_DONE) {
		return;
	}

	g_mutex_lock (data->mutex);
	data->free_blocks++;
	g_mutex_unlock (data->mutex);

	g_cond_signal (data->cond);
}
