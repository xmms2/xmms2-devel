/** @file asf.c
 *  Demuxer plugin for ASF audio format
 *  Copyright (C) 2003-2008 XMMS2 Team
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_bindata.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#include "libasf/asf.h"

#define ASF_BUFFER_SIZE 4096

typedef struct {
	asf_file_t *file;

	gint track;
	gint samplerate;
	gint channels;
	gint bitrate;

	asf_packet_t *packet;
	GString *outbuf;
} xmms_asf_data_t;

static gboolean xmms_asf_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_asf_init (xmms_xform_t *xform);
static void xmms_asf_destroy (xmms_xform_t *xform);
static gint xmms_asf_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static gint64 xmms_asf_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);
static void xmms_asf_get_mediainfo (xmms_xform_t *xform);

int32_t xmms_asf_read_callback (void *opaque, void *buffer, int32_t size);
int64_t xmms_asf_seek_callback (void *opaque, int64_t position);
gint xmms_asf_get_track (xmms_xform_t *xform, asf_file_t *file);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("asf",
                   "ASF Demuxer", XMMS_VERSION,
                   "Advanced Systems Format demuxer",
                   xmms_asf_plugin_setup);

static gboolean
xmms_asf_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_asf_init;
	methods.destroy = xmms_asf_destroy;
	methods.read = xmms_asf_read;
	methods.seek = xmms_asf_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "video/x-ms-asf",
	                              NULL);

	xmms_magic_add ("asf header", "video/x-ms-asf",
	                "0 belong 0x3026b275", NULL);

	return TRUE;
}

static void
xmms_asf_destroy (xmms_xform_t *xform)
{
	xmms_asf_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	g_string_free (data->outbuf, TRUE);
	asf_free_packet (data->packet);
	g_free (data);
}

static gboolean
xmms_asf_init (xmms_xform_t *xform)
{
	xmms_asf_data_t *data;
	asf_stream_t stream;
	gint ret;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_asf_data_t, 1);

	stream.read = xmms_asf_read_callback;
	stream.write = NULL;
	stream.seek = xmms_asf_seek_callback;
	stream.opaque = xform;

	data->file = asf_open_cb (&stream);
	if (!data->file) {
		g_free (data);
		return FALSE;
	}
	data->packet = asf_packet_create ();
	data->outbuf = g_string_new (NULL);

	xmms_xform_private_data_set (xform, data);

	ret = asf_init (data->file);
	if (ret < 0) {
		XMMS_DBG ("ASF parser failed to init with error %d", ret);
		asf_free_packet (data->packet);
		asf_close (data->file);

		return FALSE;
	}

	data->track = xmms_asf_get_track (xform, data->file);
	if (data->track < 0) {
		XMMS_DBG ("No audio track found");
		asf_free_packet (data->packet);
		asf_close (data->file);

		return FALSE;
	}

	xmms_asf_get_mediainfo (xform);

	XMMS_DBG ("ASF demuxer inited successfully!");

	return TRUE;
}

static gint
xmms_asf_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	xmms_asf_data_t *data;
	guint size;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	size = MIN (data->outbuf->len, len);
	while (size == 0) {
		gint i, ret;

		ret = asf_get_packet (data->file, data->packet);
		if (ret < 0) {
			return -1;
		} else if (ret == 0) {
			XMMS_DBG ("ASF EOF");
			return 0;
		}

		for (i=0; i<data->packet->payload_count; i++) {
			asf_payload_t *payload = &data->packet->payloads[i];

			if (payload->stream_number != data->track) {
				/* This is some stream we don't want */
				continue;
			}
			g_string_append_len (data->outbuf, (gchar *) payload->data, payload->datalen);
			xmms_xform_auxdata_barrier (xform);
		}

		size = MIN (data->outbuf->len, len);
	}

	memcpy (buf, data->outbuf->str, size);
	g_string_erase (data->outbuf, 0, size);

	return size;
}

static gint64
xmms_asf_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	int64_t position;
	xmms_asf_data_t *data;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	position = asf_seek_to_msec (data->file, samples * 1000 / data->samplerate);
	XMMS_DBG ("Seeking %" G_GINT64_FORMAT " returned with %" G_GINT64_FORMAT,
	          samples * 1000 / data->samplerate, position);
	if (position < 0) {
		return -1;
	}
	g_string_erase (data->outbuf, 0, data->outbuf->len);

	return position * data->samplerate / 1000;
}

static void
xmms_asf_get_mediainfo (xmms_xform_t *xform)
{
	xmms_asf_data_t *data;
	asf_metadata_t *metadata;
	uint64_t tmp;
	gchar *track = NULL;
	gint i;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if ((tmp = asf_get_duration (data->file)) > 0) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
		                             tmp/10000);
	}

	if ((tmp = asf_get_max_bitrate (data->file)) > 0) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
		                             tmp);
	}

	metadata = asf_get_metadata (data->file);

	if (metadata->title && metadata->title[0]) {
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,
		                             metadata->title);
	}

	if (metadata->artist && metadata->artist[0]) {
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,
		                             metadata->artist);
	}

	if (metadata->description && metadata->description[0]) {
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT,
		                             metadata->description);
	}

	for (i=0; i<metadata->extended_count; i++) {
		char *key, *value;

		key = metadata->extended[i].key;
		value = metadata->extended[i].value;

		if (key == NULL || value == NULL || !strlen (value)) {
			continue;
		} else if (!strcmp (key, "WM/AlbumTitle")) {
			xmms_xform_metadata_set_str (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,
			                             value);
		} else if (!strcmp (key, "WM/Year")) {
			xmms_xform_metadata_set_str (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,
			                             value);
		} else if (!strcmp (key, "WM/Genre")) {
			xmms_xform_metadata_set_str (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,
			                             value);
		} else if ((!track && !strcmp (key, "WM/Track")) || !strcmp (key, "WM/TrackNumber")) {
			/* WM/TrackNumber overrides WM/Track value as specified in the Microsoft
			 * documentation at http://msdn2.microsoft.com/en-us/library/aa392014.aspx */
			track = value;
		} else if (!strcmp (key, "MusicBrainz/Album Id")) {
			xmms_xform_metadata_set_str (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID,
			                             value);
		} else if (!strcmp (key, "MusicBrainz/Artist Id")) {
			xmms_xform_metadata_set_str (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID,
			                             value);
		} else if (!strcmp (key, "MusicBrainz/Track Id")) {
			xmms_xform_metadata_set_str (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID,
			                             value);
		}
	}

	if (track) {
		gint tracknr;
		gchar *end;

		tracknr = strtol (track, &end, 10);
		if (end && *end == '\0') {
			xmms_xform_metadata_set_int (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,
			                             tracknr);
		}
	}

	asf_free_metadata (metadata);
}

int32_t
xmms_asf_read_callback (void *opaque, void *buffer, int32_t size)
{
	xmms_xform_t *xform;
	xmms_asf_data_t *data;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (opaque, 0);
	g_return_val_if_fail (buffer, 0);
	xform = opaque;

	xmms_error_reset (&error);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	ret = xmms_xform_read (xform, buffer, size, &error);

	return ret;
}

int64_t
xmms_asf_seek_callback (void *opaque, int64_t position)
{
	xmms_xform_t *xform;
	xmms_asf_data_t *data;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (opaque, -1);
	xform = opaque;

	xmms_error_reset (&error);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = xmms_xform_seek (xform, position, XMMS_XFORM_SEEK_SET, &error);

	return ret;
}

gint
xmms_asf_get_track (xmms_xform_t *xform, asf_file_t *file)
{
	xmms_asf_data_t *data;
	uint8_t stream_count;
	gint i;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	stream_count = asf_get_stream_count (file);

	for (i=1; i <= stream_count; i++) {
		asf_stream_properties_t *sprop = asf_get_stream_properties (file, i);
		if (sprop->type == ASF_STREAM_TYPE_AUDIO) {
			asf_waveformatex_t *wfx = sprop->properties;
			const gchar *mimetype;

			if (wfx->wFormatTag == 0x160)
				mimetype = "audio/x-ffmpeg-wmav1";
			else if (wfx->wFormatTag == 0x161)
				mimetype = "audio/x-ffmpeg-wmav2";
			else
				continue;

			data->samplerate = wfx->nSamplesPerSec;
			data->channels = wfx->nChannels;
			data->bitrate = wfx->nAvgBytesPerSec * 8;

			xmms_xform_auxdata_set_bin (xform,
			                            "decoder_config",
			                            wfx->data,
			                            wfx->cbSize);

			xmms_xform_auxdata_set_int (xform,
			                            "block_align",
			                            wfx->nBlockAlign);

			xmms_xform_auxdata_set_int (xform,
			                            "bitrate",
			                            data->bitrate);

			xmms_xform_outdata_type_add (xform,
			                             XMMS_STREAM_TYPE_MIMETYPE,
			                             mimetype,
			                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
			                             data->samplerate,
			                             XMMS_STREAM_TYPE_FMT_CHANNELS,
			                             data->channels,
			                             XMMS_STREAM_TYPE_END);

			return i;
		}
	}

	return -1;
}

