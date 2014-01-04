/** @file mp4.c
 *  Decoder plugin for MP4 audio format
 *
 *  Copyright (C) 2005-2014 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_bindata.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#include "mp4ff/mp4ff.h"

#define MP4_BUFFER_SIZE 4096

typedef struct {
	gint filetype;

	mp4ff_t *mp4ff;
	mp4ff_callback_t *mp4ff_cb;
	gint track;
	glong sampleid;
	glong numsamples;

	guchar buffer[MP4_BUFFER_SIZE];
	guint buffer_length;
	guint buffer_size;

	GString *outbuf;
} xmms_mp4_data_t;

static gboolean xmms_mp4_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_mp4_init (xmms_xform_t *xform);
static void xmms_mp4_destroy (xmms_xform_t *xform);
static gint xmms_mp4_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static gint64 xmms_mp4_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);
static void xmms_mp4_get_mediainfo (xmms_xform_t *xform);
static gboolean xmms_mp4_mediainfo_set_coverart (xmms_xform_t *xform, const gchar *key, const gchar *value, gsize length);

static gboolean xmms_mp4_gapless_set (xmms_xform_t *xform, guint64 startsamples, guint64 stopsamples);
static gboolean xmms_mp4_gapless_itunsmpb_parse (const gchar *value, gsize length, guint64 duration, guint64 *startsamples, guint64 *stopsamples);
static gboolean xmms_mp4_gapless_try_itunsmpb (xmms_xform_t *xform);
static gboolean xmms_mp4_gapless_try_legacy (xmms_xform_t *xform);
static gboolean xmms_mp4_gapless_try (xmms_xform_t *xform);

static uint32_t xmms_mp4_read_callback (void *user_data, void *buffer, uint32_t length);
static uint32_t xmms_mp4_seek_callback (void *user_data, uint64_t position);
static int xmms_mp4_get_track (xmms_xform_t *xform, mp4ff_t *infile);

static const xmms_xform_metadata_basic_mapping_t basic_mappings[] = {
	{ "album",                       XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM             },
	{ "title",                       XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE             },
	{ "artist",                      XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST            },
	{ "album_artist",                XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST      },
	{ "date",                        XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR              },
	{ "writer",                      XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER          },
	{ "lyricist",                    XMMS_MEDIALIB_ENTRY_PROPERTY_LYRICIST          },
	{ "conductor",                   XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR         },
	{ "remixer",                     XMMS_MEDIALIB_ENTRY_PROPERTY_REMIXER           },
	{ "producer",                    XMMS_MEDIALIB_ENTRY_PROPERTY_PRODUCER          },
	{ "mixer",                       XMMS_MEDIALIB_ENTRY_PROPERTY_MIXER             },
	{ "contentgroup",                XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING          },
	{ "track",                       XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR           },
	{ "totaltracks",                 XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALTRACKS       },
	{ "disc",                        XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET         },
	{ "totaldiscs",                  XMMS_MEDIALIB_ENTRY_PROPERTY_TOTALSET          },
	{ "compilation",                 XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       },
	{ "comment",                     XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT           },
	{ "genre",                       XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE             },
	{ "tempo",                       XMMS_MEDIALIB_ENTRY_PROPERTY_BPM               },
	{ "isrc",                        XMMS_MEDIALIB_ENTRY_PROPERTY_ISRC              },
	{ "catalognumber",               XMMS_MEDIALIB_ENTRY_PROPERTY_CATALOGNUMBER     },
	{ "barcode",                     XMMS_MEDIALIB_ENTRY_PROPERTY_BARCODE           },
	{ "sortalbum",                   XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_SORT        },
	{ "sortalbumartist",             XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT },
	{ "sortartist",                  XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_SORT       },
	{ "description",                 XMMS_MEDIALIB_ENTRY_PROPERTY_DESCRIPTION       },
	{ "label",                       XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER         },
	{ "asin",                        XMMS_MEDIALIB_ENTRY_PROPERTY_ASIN              },
	{ "MusicBrainz Album Id",        XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID          },
	{ "MusicBrainz Artist Id",       XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID         },
	{ "MusicBrainz Track Id",        XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID          },
	{ "MusicBrainz Album Artist Id", XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       },
	{ "replaygain_track_gain",       XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK        },
	{ "replaygain_album_gain",       XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM        },
	{ "replaygain_track_peak",       XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK        },
	{ "replaygain_album_peak",       XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM        },
};

static const xmms_xform_metadata_mapping_t mappings[] = {
	{ "cover", xmms_mp4_mediainfo_set_coverart }
};

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("mp4",
                          "MP4 Demuxer", XMMS_VERSION,
                          "MPEG-4 Part 14 file format demuxer",
                          xmms_mp4_plugin_setup);

static gboolean
xmms_mp4_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_mp4_init;
	methods.destroy = xmms_mp4_destroy;
	methods.read = xmms_mp4_read;
	methods.seek = xmms_mp4_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_metadata_mapper_init (xform_plugin,
	                                        basic_mappings,
	                                        G_N_ELEMENTS (basic_mappings),
	                                        mappings,
	                                        G_N_ELEMENTS (mappings));

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "video/mp4",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/mp4",
	                              NULL);

	xmms_magic_add ("mpeg-4 header", "video/mp4",
	                "4 string ftyp",
	                ">8 string isom",
	                ">8 string mp41",
	                ">8 string mp42",
	                NULL);

	xmms_magic_add ("iTunes header", "audio/mp4",
	                "4 string ftyp", ">8 string M4A ", NULL);

	return TRUE;
}

static void
xmms_mp4_destroy (xmms_xform_t *xform)
{
	xmms_mp4_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->mp4ff) {
		mp4ff_close (data->mp4ff);
	}
	g_free (data->mp4ff_cb);

	g_string_free (data->outbuf, TRUE);
	g_free (data);
}

static gboolean
xmms_mp4_init (xmms_xform_t *xform)
{
	xmms_mp4_data_t *data;
	xmms_error_t error;

	gint bytes_read;

	guchar *tmpbuf;
	guint tmpbuflen;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_mp4_data_t, 1);
	data->outbuf = g_string_new (NULL);
	data->buffer_size = MP4_BUFFER_SIZE;

	xmms_xform_private_data_set (xform, data);

	data->sampleid = 0;
	data->numsamples = 0;

	bytes_read = xmms_xform_read (xform,
	                              (gchar *) data->buffer + data->buffer_length,
	                              data->buffer_size - data->buffer_length,
	                              &error);

	data->buffer_length += bytes_read;

	if (bytes_read < 8) {
		XMMS_DBG ("Not enough bytes to check the MP4 header");
		goto err;
	}

	/*
	 * MP4 not supported (yet) on non-seekable transport
	 * this needs little tweaking in mp4ff at least
	 */
	if (xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &error) < 0) {
		XMMS_DBG ("Non-seekable transport on MP4 not yet supported");
		goto err;
	}

	data->mp4ff_cb = g_new0 (mp4ff_callback_t, 1);
	data->mp4ff_cb->read = xmms_mp4_read_callback;
	data->mp4ff_cb->seek = xmms_mp4_seek_callback;
	data->mp4ff_cb->user_data = xform;

	data->mp4ff = mp4ff_open_read (data->mp4ff_cb);
	if (!data->mp4ff) {
		XMMS_DBG ("Error opening mp4 demuxer\n");
		goto err;;
	}

	data->track = xmms_mp4_get_track (xform, data->mp4ff);
	if (data->track < 0) {
		XMMS_DBG ("Can't find suitable audio track from MP4 file\n");
		goto err;
	}
	data->numsamples = mp4ff_num_samples (data->mp4ff, data->track);

	mp4ff_get_decoder_config (data->mp4ff, data->track, &tmpbuf,
	                          &tmpbuflen);
	xmms_xform_auxdata_set_bin (xform, "decoder_config", tmpbuf, tmpbuflen);
	g_free (tmpbuf);

	xmms_mp4_get_mediainfo (xform);

	xmms_mp4_gapless_try (xform);

	XMMS_DBG ("MP4 demuxer inited successfully!");

	return TRUE;

err:
	g_free (data->mp4ff_cb);
	g_string_free (data->outbuf, TRUE);
	g_free (data);

	return FALSE;
}

static gint
xmms_mp4_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	xmms_mp4_data_t *data;
	guint size, bytes_read = 0;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	size = MIN (data->outbuf->len, len);
	while (size == 0) {
		guchar *tmpbuf;
		guint tmpbuflen;
		gint duration;

		if (data->sampleid >= data->numsamples) {
			XMMS_DBG ("MP4 EOF");
			return 0;
		}

		bytes_read = mp4ff_read_sample (data->mp4ff, data->track,
		                                data->sampleid, &tmpbuf,
		                                &tmpbuflen);
		duration = mp4ff_get_sample_duration (data->mp4ff, data->track,
		                                      data->sampleid);
		data->sampleid++;

		xmms_xform_auxdata_set_int (xform, "frame_duration", duration);

		if (bytes_read > 0) {
			g_string_append_len (data->outbuf, (gchar *) tmpbuf, tmpbuflen);
			g_free (tmpbuf);
		}

		size = MIN (data->outbuf->len, len);
	}

	memcpy (buf, data->outbuf->str, size);
	g_string_erase (data->outbuf, 0, size);
	return size;
}

static gint64
xmms_mp4_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	int32_t toskip, sampleid_candidate;
	xmms_mp4_data_t *data;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	sampleid_candidate = mp4ff_find_sample(data->mp4ff, data->track, samples, &toskip);

	if (sampleid_candidate < 0) {
		return -1;
	}

	data->sampleid = sampleid_candidate;
	data->buffer_length = 0;

	g_string_erase (data->outbuf, 0, -1);

	return samples-toskip;
}

static gboolean
xmms_mp4_mediainfo_set_coverart (xmms_xform_t *xform, const gchar *key,
                                 const gchar *value, gsize length)
{
	const gchar *metakey;
	gchar hash[33];

	if (!xmms_bindata_plugin_add ((const guchar *) value, length, hash)) {
		return FALSE;
	}

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT;
	xmms_xform_metadata_set_str (xform, metakey, hash);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT_MIME;
	xmms_xform_metadata_set_str (xform, metakey, "image/jpeg");

	return TRUE;
}

static void
xmms_mp4_get_mediainfo (xmms_xform_t *xform)
{
	xmms_mp4_data_t *data;
	const gchar *metakey;
	glong temp;
	gint i, num_items;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if ((temp = mp4ff_get_sample_rate (data->mp4ff, data->track)) > 0) {
		glong srate = temp;

		if ((temp = mp4ff_get_track_duration (data->mp4ff, data->track)) >= 0) {
			glong msec = ((gint64) temp) * 1000 / srate;

			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			xmms_xform_metadata_set_int (xform, metakey, msec);
		}
	}

	if ((temp = mp4ff_get_avg_bitrate (data->mp4ff, data->track)) >= 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
		xmms_xform_metadata_set_int (xform, metakey, temp);
	}

	num_items = mp4ff_meta_get_num_items (data->mp4ff);
	for (i = 0; i < num_items; i++) {
		gchar *key, *value;
		guint length;

		length = mp4ff_meta_get_by_index (data->mp4ff, i, &key, &value);
		if (length > 0) {
			if (!xmms_xform_metadata_mapper_match (xform, key, value, length)) {
				/* iTunSMPB should be handled in xmms_mp4_gapless_try */
				if (0 != g_ascii_strcasecmp (key, "iTunSMPB")) {
					XMMS_DBG ("Unhandled tag '%s' = '%s'", key, value);
				}
			}
			g_free (key);
			g_free (value);
		}
	}
}

static gboolean
xmms_mp4_gapless_set (xmms_xform_t *xform, guint64 startsamples, guint64 stopsamples)
{
	g_return_val_if_fail ((gint64)startsamples >= 0, FALSE);
	g_return_val_if_fail ((gint64)stopsamples >= 0, FALSE);
	g_return_val_if_fail (startsamples <= stopsamples, FALSE);

	xmms_xform_auxdata_set_int (xform, "startsamples", startsamples);
	xmms_xform_auxdata_set_int (xform, "stopsamples", stopsamples);

	return TRUE;
}

static gboolean
xmms_mp4_gapless_itunsmpb_parse (const gchar *value, gsize length,
                                 guint64 duration,
                                 guint64 *startsamples, guint64 *stopsamples)
{
	gint i;
	gchar *begin, *end, *cpy;
	guint64 tagdata[12];
	guint64 enc_delay, padding, orig_count;

	/* parse the hexadecimal data */
	cpy = strndup(value, length);
	begin = NULL;
	end = cpy;
	for(i = 0; i < 12 && begin != end; i++) {
		begin = end;
		tagdata[i] = g_ascii_strtoull (begin, &end, 16);
	}
	if (begin == end || i < 12 || *end != '\0') {
		XMMS_DBG ("Could not parse iTunSMPB data!");
		g_free (cpy);
		return FALSE;
	} else {
		g_free (cpy);
	}

	/* extract interesting values */
	enc_delay = tagdata[1];
	padding = tagdata[2];
	orig_count = tagdata[3];
	if (duration > 0 && enc_delay + orig_count + padding != duration) {
		XMMS_DBG ("Data from iTunSMPB disagrees with provided duration!");
		return FALSE;
	}

	*startsamples = enc_delay;
	*stopsamples = enc_delay + orig_count;
	return TRUE;
}

static gboolean
xmms_mp4_gapless_try_itunsmpb (xmms_xform_t *xform)
{
	xmms_mp4_data_t *data;
	char *val;
	gint len;
	guint64 duration, startsamples, stopsamples;

	g_return_val_if_fail (xform, FALSE);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	len = mp4ff_meta_find_by_name(data->mp4ff, "iTunSMPB", &val);
	if (!len) {
		return FALSE;
	}

	duration = mp4ff_get_track_duration (data->mp4ff, data->track);
	if (!xmms_mp4_gapless_itunsmpb_parse (val, len, duration,
	                                      &startsamples, &stopsamples)) {
		return FALSE;
	}

	return xmms_mp4_gapless_set (xform, startsamples, stopsamples);
}

static gboolean
xmms_mp4_gapless_try_legacy (xmms_xform_t *xform)
{
	xmms_mp4_data_t *data;
	guint64 startsamples, stopsamples;

	g_return_val_if_fail (xform, FALSE);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	startsamples = mp4ff_get_sample_offset (data->mp4ff, data->track, 0);
	stopsamples = mp4ff_get_track_duration (data->mp4ff, data->track);

	return xmms_mp4_gapless_set (xform, startsamples, stopsamples);
}

static gboolean
xmms_mp4_gapless_try (xmms_xform_t *xform) {
	g_return_val_if_fail (xform, FALSE);

	/* TODO: Using Edit List box */

	/* Using iTunSMPB */
	if (xmms_mp4_gapless_try_itunsmpb (xform)) {
		XMMS_DBG ("Got startms and stopms from iTunSMPB.");
		return TRUE;
	}

	/* Using ctts and stts (legacy) */
	if (xmms_mp4_gapless_try_legacy (xform)) {
		XMMS_DBG ("Got startms and stopms from ctts and stts (legacy).");
		return TRUE;
	}

	return FALSE;
}

static uint32_t
xmms_mp4_read_callback (void *user_data, void *buffer, uint32_t length)
{
	xmms_xform_t *xform;
	xmms_mp4_data_t *data;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (user_data, 0);
	g_return_val_if_fail (buffer, 0);
	xform = user_data;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	if (data->buffer_length == 0) {
		guint bytes_read;

		bytes_read = xmms_xform_read (xform, (gchar *) data->buffer,
		                              data->buffer_size, &error);

		if (bytes_read <= 0 && data->buffer_length == 0) {
			return bytes_read;
		}

		data->buffer_length += bytes_read;
	}

	ret = MIN (length, data->buffer_length);
	g_memmove (buffer, data->buffer, ret);
	g_memmove (data->buffer, data->buffer + ret, data->buffer_length - ret);
	data->buffer_length -= ret;

	return ret;
}

static uint32_t
xmms_mp4_seek_callback (void *user_data, uint64_t position)
{
	xmms_xform_t *xform;
	xmms_mp4_data_t *data;
	xmms_error_t error;
	gint ret = 0;

	g_return_val_if_fail (user_data, -1);
	xform = user_data;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = xmms_xform_seek (xform, position, XMMS_XFORM_SEEK_SET, &error);

	/* If seeking was successfull, flush the internal buffer */
	if (ret >= 0) {
		data->buffer_length = 0;
	}

	return ret;
}

static int
xmms_mp4_get_track (xmms_xform_t *xform, mp4ff_t *infile)
{
	glong chans, rate;
	int i;
	int numTracks = mp4ff_total_tracks (infile);

	/* find first suitable audio track */
	for (i = 0; i < numTracks; i++) {
		gint object_type = mp4ff_get_audio_type (infile, i);

		/* these identifiers are mostly from VLC code */
		switch (object_type) {
		case 0x40: /* MPEG-4 audio */
		case 0x66: /* MPEG-2 AAC */
		case 0x67: /* MPEG-2 AAC LC */
		case 0x68: /* MPEG-2 AAC SSR */
			xmms_xform_outdata_type_add (xform,
			                             XMMS_STREAM_TYPE_MIMETYPE,
			                             "audio/aac",
			                             XMMS_STREAM_TYPE_END);
			return i;
		case 0x69: /* MPEG-2 audio */
		case 0x6B: /* MPEG-1 audio */
			continue;
		case 0xff: /* ALAC */
			chans = mp4ff_get_channel_count (infile, i);
			rate = mp4ff_get_sample_rate (infile, i);
			if (chans <= 0 || rate <= 0) {
				XMMS_DBG ("Bad ALAC audio track %d", i);
				continue;
			}
			xmms_xform_outdata_type_add (xform,
			                             XMMS_STREAM_TYPE_MIMETYPE,
			                             "audio/x-ffmpeg-alac",
			                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
			                             (int)rate,
			                             XMMS_STREAM_TYPE_FMT_CHANNELS,
			                             (int)chans,
			                             XMMS_STREAM_TYPE_END);
			return i;
		default:
			continue;
		}
	}

	/* can't decode this */
	return -1;
}
