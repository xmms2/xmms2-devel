/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2006-2009 XMMS2 Team
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wavpack/wavpack.h>

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

typedef enum { STRING, INTEGER, INTEGERSLASH, RPGAIN } ptype;
typedef struct {
	const gchar *vname;
	const gchar *xname;
	ptype type;
} props;

/*
 *  Try to map as many APEv2 tag keys to medialib as possible.
 *  If a medialib key is listed twice, the one furthest down the list takes priority
 */
static const props properties[] = {
	/* foobar2000 free-form strings (not in APEv2 spec) */
	{ "tracknumber",           XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,      STRING },
	{ "discnumber",            XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET,    STRING },
	{ "album artist",          XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST, STRING },

	/* Standard APEv2 tags (defined in the spec) */
	{ "title",                 XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,        STRING },
	{ "artist",                XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,       STRING },
	{ "album",                 XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,        STRING },
	{ "publisher",             XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER,    STRING },
	{ "conductor",             XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR,    STRING },
	{ "track",                 XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,      STRING },
	{ "composer",              XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER,     STRING },
	{ "comment",               XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT,      STRING },
	{ "copyright",             XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT,    STRING },
	{ "file",                  XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_FILE, STRING },
	{ "year",                  XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,         STRING },
	{ "genre",                 XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,        STRING },
	{ "media",                 XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET,    STRING },
	{ "language",              XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT_LANG, STRING },

	/* ReplayGain (including obsolete tag names - priority to new style tags) */
	{ "rg_audiophile",         XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,   RPGAIN },
	{ "replaygain_album_gain", XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,   RPGAIN },
	{ "replaygain_album_peak", XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM,   RPGAIN },
	{ "rg_radio",              XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK,   RPGAIN },
	{ "replaygain_track_gain", XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK,   RPGAIN },
	{ "rg_peak",               XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,   RPGAIN },
	{ "replaygain_track_peak", XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,   RPGAIN }
};

/*
 * Function prototypes
 */

static gboolean xmms_wavpack_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_wavpack_init (xmms_xform_t *xform);
static void xmms_wavpack_get_tags (xmms_xform_t *xform);
static gint64 xmms_wavpack_seek (xmms_xform_t *xform, gint64 samples,
                                 xmms_xform_seek_mode_t whence, xmms_error_t *error);
static gint xmms_wavpack_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                               xmms_error_t *error);
static void xmms_wavpack_destroy (xmms_xform_t *xform);

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

	xmms_wavpack_get_tags (xform);

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

static void
xmms_wavpack_get_tags (xmms_xform_t *xform)
{
	xmms_wavpack_data_t *data;
	xmms_error_t error;

	guint i = 0;
	gchar value[255];
	gchar buf[8];

	XMMS_DBG ("xmms_wavpack_get_tags");

	g_return_if_fail (xform);

	data = (xmms_wavpack_data_t *) xmms_xform_private_data_get (xform);

	xmms_error_reset (&error);

	if (WavpackGetMode (data->ctx) & MODE_VALID_TAG) {
		memset (value, 0, 255);

		for (i = 0; i < G_N_ELEMENTS (properties); i++) {
			if (WavpackGetTagItem (data->ctx, properties[i].vname, value, sizeof (value))) {
				if (properties[i].type == INTEGER) {
					gint tmp = strtol (value, NULL, 10);
					xmms_xform_metadata_set_int (xform, properties[i].xname, tmp);
				} else if (properties[i].type == RPGAIN) {
					g_snprintf (buf, sizeof (buf), "%f",
					            pow (10.0, g_strtod (value, NULL) / 20));

					xmms_xform_metadata_set_str (xform, properties[i].xname, buf);
				} else {
					xmms_xform_metadata_set_str (xform, properties[i].xname, value);
				}
			}
		}
	}
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

	if (data->buf) {
		g_free (data->buf);
	}

	g_free (data);
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
