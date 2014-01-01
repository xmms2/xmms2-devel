/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2006-2014 XMMS2 Team
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

/**
  * @file WavPack decoder
  * @url http://wavpack.com
  */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wavpack/wavpack.h>

#include "../apev2_common/apev2.c"

typedef struct xmms_wavpack_data_St {
	WavpackContext *ctx;
	WavpackStreamReader reader;

	gint channels;
	gint bits_per_sample;
	gint num_samples;

	guint8 pushback_byte;
	gboolean pushback_set;

	xmms_samples32_t *buf;
	gint buf_mono_samples;
} xmms_wavpack_data_t;

/** These are the properties that we extract from the comments */
static const xmms_xform_metadata_basic_mapping_t basic_mappings[] = {
	{ "Album",                     XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM             },
	{ "Title",                     XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE             },
	{ "Artist",                    XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST            },
	{ "Album Artist",              XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST      },
	{ "Track",                     XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR           },
	{ "Disc",                      XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET         },
	{ "Year",                      XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR              },
	{ "Composer",                  XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER          },
	{ "Lyricist",                  XMMS_MEDIALIB_ENTRY_PROPERTY_LYRICIST          },
	{ "Conductor",                 XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR         },
	{ "Performer",                 XMMS_MEDIALIB_ENTRY_PROPERTY_PERFORMER         },
	{ "MixArtist",                 XMMS_MEDIALIB_ENTRY_PROPERTY_REMIXER           },
	{ "Arranger",                  XMMS_MEDIALIB_ENTRY_PROPERTY_ARRANGER          },
	{ "Producer",                  XMMS_MEDIALIB_ENTRY_PROPERTY_PRODUCER          },
	{ "Mixer",                     XMMS_MEDIALIB_ENTRY_PROPERTY_MIXER             },
	{ "Grouping",                  XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING          },
	{ "Compilation",               XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       },
	{ "Comment",                   XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT           },
	{ "Genre",                     XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE             },
	{ "BPM",                       XMMS_MEDIALIB_ENTRY_PROPERTY_BPM               },
	{ "ASIN",                      XMMS_MEDIALIB_ENTRY_PROPERTY_ASIN              },
	{ "ISRC",                      XMMS_MEDIALIB_ENTRY_PROPERTY_ISRC              },
	{ "Label",                     XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER         },
	{ "Copyright",                 XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT         },
	{ "CatalogNumber",             XMMS_MEDIALIB_ENTRY_PROPERTY_CATALOGNUMBER     },
	{ "Barcode",                   XMMS_MEDIALIB_ENTRY_PROPERTY_BARCODE           },
	{ "ALBUMSORT",                 XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_SORT        },
	{ "ALBUMARTISTSORT",           XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST_SORT },
	{ "ARTISTSORT",                XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_SORT       },
	{ "MUSICBRAINZ_TRACKID",       XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID          },
	{ "MUSICBRAINZ_ALBUMID",       XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID          },
	{ "MUSICBRAINZ_ARTISTID",      XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID         },
	{ "MUSICBRAINZ_ALBUMARTISTID", XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION       },

	/* foobar2000 free-form strings (not in APEv2 spec) */
	{ "tracknumber",               XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,          },
	{ "discnumber",                XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET,        },

	/* ReplayGain (including obsolete tag names - priority to new style tags) */
	{ "rg_audiophile",             XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,       },
	{ "replaygain_album_gain",     XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,       },
	{ "replaygain_album_peak",     XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM,       },
	{ "rg_radio",                  XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK,       },
	{ "replaygain_track_gain",     XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK,       },
	{ "rg_peak",                   XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,       },
	{ "replaygain_track_peak",     XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,       }
};

static const xmms_xform_metadata_mapping_t mappings[] = {
	{ "Cover Art (Front)", xmms_apetag_handle_tag_coverart }
};

/*
 * Function prototypes
 */

static gboolean xmms_wavpack_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_wavpack_init (xmms_xform_t *xform);
static gint64 xmms_wavpack_seek (xmms_xform_t *xform, gint64 samples,
                                 xmms_xform_seek_mode_t whence, xmms_error_t *error);
static gint xmms_wavpack_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                               xmms_error_t *error);
static void xmms_wavpack_destroy (xmms_xform_t *xform);

static void xmms_wavpack_free_data (xmms_wavpack_data_t *data);
static int32_t wavpack_read_bytes (void *id, void *data, int32_t bcount);
static uint32_t wavpack_get_pos (void *id);
static int wavpack_set_pos_abs (void *id, uint32_t pos);
static int wavpack_set_pos_rel (void *id, int32_t pos, int whence);
static int wavpack_push_back_byte (void *id, int c);
static uint32_t wavpack_get_length (void *id);
static int wavpack_can_seek (void *id);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("wavpack",
                   "WavPack Decoder", XMMS_VERSION,
                   "WavPack decoder",
                   xmms_wavpack_plugin_setup);

static gboolean
xmms_wavpack_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_wavpack_init;
	methods.destroy = xmms_wavpack_destroy;
	methods.read = xmms_wavpack_read;
	methods.seek = xmms_wavpack_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_metadata_mapper_init (xform_plugin,
	                                        basic_mappings,
	                                        G_N_ELEMENTS (basic_mappings),
	                                        mappings,
	                                        G_N_ELEMENTS (mappings));

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-wavpack",
	                              NULL);

	xmms_magic_add ("wavpack header v4", "audio/x-wavpack",
	                "0 string wvpk", NULL);

	xmms_magic_extension_add ("audio/x-wavpack", "*.wv");

	return TRUE;
}

static gboolean
xmms_wavpack_init (xmms_xform_t *xform)
{
	xmms_wavpack_data_t *data;
	xmms_sample_format_t sample_format;
	gint samplerate;
	/* the maximum length of error really isn't defined... stupid */
	gchar error[1024];

	g_return_val_if_fail (xform, FALSE);

	if (!xmms_apetag_read (xform)) {
		XMMS_DBG ("Failed to read APEv2 tag");
	}

	data = g_new0 (xmms_wavpack_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_xform_private_data_set (xform, data);

	data->reader.read_bytes = wavpack_read_bytes;
	data->reader.get_pos = wavpack_get_pos;
	data->reader.set_pos_abs = wavpack_set_pos_abs;
	data->reader.set_pos_rel = wavpack_set_pos_rel;
	data->reader.push_back_byte = wavpack_push_back_byte;
	data->reader.get_length = wavpack_get_length;
	data->reader.can_seek = wavpack_can_seek;

	data->ctx = WavpackOpenFileInputEx (&data->reader,
	                                    xform, xform,
	                                    error, OPEN_TAGS, 0);

	if (!data->ctx) {
		xmms_log_error ("Unable to open wavpack file: %s", error);
		xmms_xform_private_data_set (xform, NULL);
		xmms_wavpack_free_data (data);
		return FALSE;
	}

	data->channels = WavpackGetNumChannels (data->ctx);
	data->bits_per_sample = WavpackGetBitsPerSample (data->ctx);
	data->num_samples = WavpackGetNumSamples (data->ctx);
	samplerate = WavpackGetSampleRate (data->ctx);

	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
	                             (int) (1000.0 * data->num_samples / samplerate));
	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE,
	                             samplerate);
	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
	                             (int) WavpackGetAverageBitrate (data->ctx, FALSE));

	switch (data->bits_per_sample) {
	case 8:
		sample_format = XMMS_SAMPLE_FORMAT_S8;
		break;
	case 12:
	case 16:
		sample_format = XMMS_SAMPLE_FORMAT_S16;
		break;
	case 24:
	case 32:
		sample_format = XMMS_SAMPLE_FORMAT_S32;
		break;
	default:
		xmms_log_error ("Unsupported bits-per-sample in wavpack file: %d",
		                data->bits_per_sample);
		xmms_xform_private_data_set (xform, NULL);
		xmms_wavpack_free_data (data);
		return FALSE;
	}

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             sample_format,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             samplerate,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gint64
xmms_wavpack_seek (xmms_xform_t *xform, gint64 samples,
              xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	xmms_wavpack_data_t *data;
	gint ret;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (samples >= 0, -1);
	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = WavpackSeekSample (data->ctx, (uint32_t)samples);

	if (ret) {
		/* success */
		return samples;
	} else {
		return -1;
	}
}

static void
xmms_wavpack_ensure_buf (xmms_wavpack_data_t *data, gint mono_samples)
{
	if (data->buf_mono_samples < mono_samples) {
		data->buf = g_realloc (data->buf,
		                       mono_samples * sizeof (xmms_samples32_t));
		data->buf_mono_samples = mono_samples;
	}
}

static gint
xmms_wavpack_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
              xmms_error_t *error)
{
	xmms_wavpack_data_t *data;
	gint mono_samples, samples;
	xmms_samples32_t *buf32;
	gint i;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	switch (data->bits_per_sample) {
	case 8:
		mono_samples = len;
		xmms_wavpack_ensure_buf (data, mono_samples);
		buf32 = data->buf;
		break;
	case 12:
	case 16:
		mono_samples = len / 2;
		xmms_wavpack_ensure_buf (data, mono_samples);
		buf32 = data->buf;
		break;
	case 24:
		mono_samples = len / 4;
		buf32 = buf;
		break;
	case 32:
		mono_samples = len / 4;
		buf32 = buf;
		break;
	}

	samples = mono_samples / data->channels;

	samples = WavpackUnpackSamples (data->ctx, buf32, samples);

	mono_samples = samples * data->channels;

	switch (data->bits_per_sample) {
	case 8:
		len = mono_samples;
		for (i=0; i<mono_samples; ++i) {
			((xmms_samples8_t *) buf)[i] = data->buf[i];
		}
		break;
	case 12:
		len = mono_samples * 2;
		for (i=0; i<mono_samples; ++i) {
			((xmms_samples16_t *) buf)[i] = data->buf[i] << 4;
		}
		break;
	case 16:
		len = mono_samples * 2;
		for (i=0; i<mono_samples; ++i) {
			((xmms_samples16_t *) buf)[i] = data->buf[i];
		}
		break;
	case 24:
		len = mono_samples * 4;
		for (i=0; i<mono_samples; ++i) {
			((xmms_samples32_t *) buf)[i] <<= 8;
		}
		break;
	case 32:
		len = mono_samples * 4;
		break;
	}

	return len;
}

static void
xmms_wavpack_destroy (xmms_xform_t *xform)
{
	xmms_wavpack_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	xmms_wavpack_free_data (data);
}

static int32_t
wavpack_read_bytes (void *id, void *buf, int32_t bcount)
{
	xmms_xform_t *xform = id;
	xmms_wavpack_data_t *data;
	xmms_error_t error;
	gint64 ret;
	gboolean did_pushback = FALSE;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	/* if we have pushback data, consume it */
	if (data->pushback_set && bcount > 0) {
		((guint8 *)buf)[0] = data->pushback_byte;
		buf++;
		bcount--;

		data->pushback_set = FALSE;

		did_pushback = TRUE;

		if (bcount == 0) {
			return 1;
		}
	}

	ret = xmms_xform_read (xform, buf, bcount, &error);

	if (ret != -1 && did_pushback) {
		/* adjust return value if we consumed the pushback byte */
		ret++;
	}

	return ret;
}

static uint32_t
wavpack_get_pos (void *id)
{
	xmms_xform_t *xform = id;
	xmms_wavpack_data_t *data;
	xmms_error_t error;

	g_return_val_if_fail (xform, (uint32_t)-1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, (uint32_t)-1);

	gint64 pos = xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &error);

	if (data->pushback_set) {
		/* we didn't actually perform the pushback on the
		   underlying stream so adjust offset accordingly */
		pos--;
	}

	return (uint32_t)pos;
}

/* return 0 on success, -1 on error */
static int
wavpack_set_pos_abs (void *id, uint32_t pos)
{
	xmms_xform_t *xform = id;
	xmms_wavpack_data_t *data;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = xmms_xform_seek (xform, pos, XMMS_XFORM_SEEK_SET, &error);

	if (ret == -1) {
		return -1;
	}

	data->pushback_set = FALSE;

	return 0;
}

/* return 0 on success, -1 on error */
static int
wavpack_set_pos_rel (void *id, int32_t pos, int whence)
{
	xmms_xform_t *xform = id;
	xmms_wavpack_data_t *data;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	if (whence == SEEK_CUR) {
		whence = XMMS_XFORM_SEEK_CUR;

		if (data->pushback_set) {
			/* we didn't actually perform the pushback on the
			   underlying stream so adjust offset accordingly */
			pos--;
		}
	} else if (whence == SEEK_SET) {
		whence = XMMS_XFORM_SEEK_SET;
	} else if (whence == SEEK_END) {
		whence = XMMS_XFORM_SEEK_END;
	}

	ret = xmms_xform_seek (xform, pos, whence, &error);

	data->pushback_set = FALSE;

	return ((ret != -1) ? 0 : -1);
}

static int
wavpack_push_back_byte (void *id, int c)
{
	xmms_xform_t *xform = id;
	xmms_wavpack_data_t *data;

	g_return_val_if_fail (xform, EOF);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, EOF);

	if (data->pushback_set) {
		/* current implementation only supports pushing back one byte,
		   and in wavpack-4.31 this is enough */
		/* => return failure */
		return EOF;
	}

	data->pushback_byte = c;
	data->pushback_set = TRUE;

	return c;
}

static uint32_t
wavpack_get_length (void *id)
{
	xmms_xform_t *xform = id;
	gint filesize;
	const gchar *metakey;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (!xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		filesize = 0;
	}

	return filesize;
}

static int
wavpack_can_seek (void *id)
{
	xmms_xform_t *xform = id;
	xmms_error_t error;
	int ret;

	ret = xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &error);

	return (ret != -1);
}

static void
xmms_wavpack_free_data (xmms_wavpack_data_t *data)
{
	if (!data) {
		return;
	}

	if (data->buf) {
		g_free (data->buf);
	}

	if (data->ctx) {
		WavpackCloseFile (data->ctx);
	}

	g_free (data);
}
