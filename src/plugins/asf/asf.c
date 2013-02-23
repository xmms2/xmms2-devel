/** @file asf.c
 *  Demuxer plugin for ASF audio format
 *  Copyright (C) 2003-2013 XMMS2 Team
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

static gboolean xmms_asf_handle_tag_coverart (xmms_xform_t *xform, const gchar *key, const gchar *value, gsize length);
static gboolean xmms_asf_handle_tag_old_tracknr (xmms_xform_t *xform, const gchar *key, const gchar *value, gsize length);
static gboolean xmms_asf_handle_tag_is_vbr (xmms_xform_t *xform, const gchar *key, const gchar *value, gsize length);

int32_t xmms_asf_read_callback (void *opaque, void *buffer, int32_t size);
int64_t xmms_asf_seek_callback (void *opaque, int64_t position);
gint xmms_asf_get_track (xmms_xform_t *xform, asf_file_t *file);

static const xmms_xform_metadata_basic_mapping_t basic_mappings[] = {
	{ "WM/AlbumTitle",               XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM             },
	{ "WM/AlbumArtist",              XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST      },
	{ "WM/OriginalArtist",           XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINAL_ARTIST   },
	{ "WM/Year",                     XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR              },
	{ "WM/OriginalReleaseYear",      XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINALYEAR      },
	{ "WM/Composer",                 XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER          },
	{ "WM/Writer",                   XMMS_MEDIALIB_ENTRY_PROPERTY_LYRICIST          },
	{ "WM/Conductor",                XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR         },
	{ "WM/ModifiedBy",               XMMS_MEDIALIB_ENTRY_PROPERTY_REMIXER           },
	{ "WM/Producer",                 XMMS_MEDIALIB_ENTRY_PROPERTY_PRODUCER          },
	{ "WM/ContentGroupDescription",  XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING          },
	{ "WM/TrackNumber",              XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR           },
	{ "WM/PartOfSet",                XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET         },
	{ "WM/Genre",                    XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE             },
	{ "WM/BeatsPerMinute",           XMMS_MEDIALIB_ENTRY_PROPERTY_BPM               },
	{ "WM/ISRC",                     XMMS_MEDIALIB_ENTRY_PROPERTY_ISRC              },
	{ "WM/Publisher",                XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER         },
	{ "WM/CatalogNo",                XMMS_MEDIALIB_ENTRY_PROPERTY_CATALOGNUMBER     },
	{ "WM/Barcode",                  XMMS_MEDIALIB_ENTRY_PROPERTY_BARCODE           },
	{ "WM/AlbumSortOrder",           XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_SORT        },
	{ "WM/AlbumArtistSortOrder",     XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT },
	{ "WM/ArtistSortOrder",          XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_SORT       },
	{ "Year",                        XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR              },
	{ "Copyright",                   XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT         },
	{ "Station",                     XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNEL           },
	{ "MusicBrainz/Album Id",        XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID          },
	{ "MusicBrainz/Artist Id",       XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID         },
	{ "MusicBrainz/Track Id",        XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID          },
	{ "MusicBrainz/Album Artist Id", XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       },
};

static const xmms_xform_metadata_mapping_t mappings[] = {
	{ "WM/Picture", xmms_asf_handle_tag_coverart    },
	{ "WM/Track",   xmms_asf_handle_tag_old_tracknr },
	{ "IsVBR",      xmms_asf_handle_tag_is_vbr      },
};

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

	xmms_xform_plugin_metadata_mapper_init (xform_plugin,
	                                        basic_mappings,
	                                        G_N_ELEMENTS (basic_mappings),
	                                        mappings,
	                                        G_N_ELEMENTS (mappings));

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
	asf_packet_destroy (data->packet);
	g_free (data);
}

static gboolean
xmms_asf_init (xmms_xform_t *xform)
{
	xmms_asf_data_t *data;
	asf_iostream_t stream;
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
		asf_packet_destroy (data->packet);
		asf_close (data->file);

		return FALSE;
	}

	data->track = xmms_asf_get_track (xform, data->file);
	if (data->track < 0) {
		XMMS_DBG ("No audio track found");
		asf_packet_destroy (data->packet);
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
	gint64 position;
	xmms_asf_data_t *data;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	position = asf_seek_to_msec (data->file, samples * 1000 / data->samplerate);
	XMMS_DBG ("Seeking %" G_GINT64_FORMAT " returned with %" G_GINT64_FORMAT,
	          samples * 1000 / data->samplerate, position);
	if (position < 0) {
		return -1;
	}
	g_string_erase (data->outbuf, 0, data->outbuf->len);

	return position * data->samplerate / 1000;
}

static gsize
xmms_asf_utf16_strnlen (const gchar *buf, gsize max_length)
{
	gint i;

	for (i = 0; i < (max_length - 1); i += 2) {
		if (buf[i] == '\0' && buf[i + 1] == '\0') {
			return i;
		}
	}

	return max_length;
}

static gboolean
xmms_asf_handle_tag_coverart (xmms_xform_t *xform, const gchar *key,
                              const gchar *value, gsize length)
{
	const guint8 *uptr;
	const gchar *ptr;
	gchar *mime;
	gchar hash[33];
	guint32 picture_length;
	gsize size;
	GError *err = NULL;

	uptr = (const guchar *) value;

	/* check picture type */
	if (uptr[0] != 0x00 && uptr[0] != 0x03) {
		return FALSE;
	}

	/* step past picture type */
	uptr++;

	picture_length =
		((guint32) uptr[3] << 24) |
		((guint32) uptr[2] << 16) |
		((guint32) uptr[1] <<  8) |
		((guint32) uptr[0]);
	if (picture_length == 0) {
		return FALSE;
	}

	/* step past picture size */
	uptr += sizeof (guint32);

	ptr = (const gchar *) uptr;

	/* parse the UTF-16 mime type */
	size = xmms_asf_utf16_strnlen (ptr, (value + length) - ptr);
	mime = g_convert (ptr, size, "UTF-8", "UTF-16", NULL, NULL, &err);
	ptr += size + 2;

	if (mime == NULL || mime[0] == '\0') {
		return FALSE;
	}

	/* step past picture description */
	size = xmms_asf_utf16_strnlen (ptr, (value + length) - ptr);
	ptr += size + 2;

	uptr = (const guchar *) ptr;

	if (xmms_bindata_plugin_add (uptr, picture_length, hash)) {
		const gchar *metakey;

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT;
		xmms_xform_metadata_set_str (xform, metakey, hash);

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT_MIME;
		xmms_xform_metadata_set_str (xform, metakey, mime);
	}

	g_free (mime);

	return TRUE;
}


static gboolean
xmms_asf_handle_tag_is_vbr (xmms_xform_t *xform, const gchar *key,
                            const gchar *value, gsize length)
{
	if (strcasecmp ("true", value) == 0) {
		xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_IS_VBR, 1);
		return TRUE;
	}
	return FALSE;
}

static gboolean
xmms_asf_handle_tag_old_tracknr (xmms_xform_t *xform, const gchar *key,
                                 const gchar *value, gsize length)
{
	gint ivalue;

	/* WM/TrackNumber overrides WM/Track value as specified in the Microsoft
	 * documentation at http://msdn2.microsoft.com/en-us/library/aa392014.aspx
	 * so lets check if something else has set the tracknr property before us.
	 */
	if (xmms_xform_metadata_get_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, &ivalue)) {
		return FALSE;
	}

	/* Ok, nothing set, lets handle "WM/Track" as "WM/TrackNumber" */
	if (!xmms_xform_metadata_mapper_match (xform, "WM/TrackNumber", value, length)) {
		return FALSE;
	}

	/* Last quirk, WM/Track is 0-indexed, need to fix that */
	xmms_xform_metadata_get_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, &ivalue);
	xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR, ivalue + 1);

	return TRUE;
}

static void
xmms_asf_get_mediainfo (xmms_xform_t *xform)
{
	xmms_asf_data_t *data;
	asf_metadata_t *metadata;
	uint64_t tmp;
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

	metadata = asf_header_get_metadata (data->file);
	if (!metadata) {
		XMMS_DBG ("No metadata object found in the file");
		return;
	}

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

	for (i = 0; i < metadata->extended_count; i++) {
		const char *key, *value;
		guint16 length;

		key = metadata->extended[i].key;
		value = metadata->extended[i].value;
		length = metadata->extended[i].length;

		if (!xmms_xform_metadata_mapper_match (xform, key, value, length)) {
			XMMS_DBG ("Unhandled tag '%s' = '%s'", key, value);
		}
	}

	asf_metadata_destroy (metadata);
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
		asf_stream_t *stream = asf_get_stream (file, i);
		if (stream->type == ASF_STREAM_TYPE_AUDIO &&
		    !(stream->flags & ASF_STREAM_FLAG_HIDDEN)) {
			asf_waveformatex_t *wfx = stream->properties;
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
