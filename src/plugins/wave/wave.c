/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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
  * @file wave decoder.
  * @url http://msdn.microsoft.com/library/en-us/dnnetcomp/html/WaveInOut.asp?frame=true#waveinout_topic_003
  */


#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>

#include <glib.h>
#include <string.h>

typedef struct xmms_wave_data_St {
	guint16 channels;
	guint32 samplerate;
	guint16 bits_per_sample;
	guint header_size;
	guint bytes_total;
} xmms_wave_data_t;

typedef enum {
	WAVE_FORMAT_UNDEFINED,
	WAVE_FORMAT_PCM,
	WAVE_FORMAT_MP3 = 0x55
} xmms_wave_format_t;

/*
 * Defines
 */
#define WAVE_HEADER_MIN_SIZE 44

#define GET_16(buf, val) \
	g_assert (sizeof (val) == 2); \
	memcpy (&val, buf, 2); \
	buf += 2; \
	bytes_left -= 2; \
	val = GUINT16_TO_LE (val);

#define GET_32(buf, val) \
	g_assert (sizeof (val) == 4); \
	memcpy (&val, buf, 4); \
	buf += 4; \
	bytes_left -= 4; \
	val = GUINT32_TO_LE (val);

#define GET_STR(buf, str, len) \
	strncpy ((gchar *) str, (gchar *)buf, len); \
	str[len] = '\0'; \
	bytes_left -= len; \
	buf += len;

#define SKIP(len) \
	bytes_left -= len; \
	buf += len;

/*
 * Function prototypes
 */

static gboolean xmms_wave_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_wave_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                            xmms_error_t *error);
static void xmms_wave_get_media_info (xmms_xform_t *xform);
static void xmms_wave_destroy (xmms_xform_t *xform);
static gboolean xmms_wave_init (xmms_xform_t *xform);
static gint64 xmms_wave_seek (xmms_xform_t *xform, gint64 samples,
                              xmms_xform_seek_mode_t whence,
                              xmms_error_t *error);

static xmms_wave_format_t read_wave_header (xmms_wave_data_t *data,
                                            guint8 *buf, gint bytes_read);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("wave",
                          "Wave Decoder", XMMS_VERSION,
                          "Wave decoder",
                          xmms_wave_plugin_setup);

static gboolean
xmms_wave_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_wave_init;
	methods.destroy = xmms_wave_destroy;
	methods.read = xmms_wave_read;
	methods.seek = xmms_wave_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-wav",
	                              XMMS_STREAM_TYPE_END);

	xmms_magic_add ("wave header", "audio/x-wav",
	                "0 string RIFF", ">8 string WAVE",
	                ">>12 string fmt ", NULL);

	return TRUE;
}

static void
xmms_wave_get_media_info (xmms_xform_t *xform)
{
	xmms_wave_data_t *data;
	gdouble playtime;
	guint samples_total, bitrate;
	gint filesize;
	const gchar *metakey;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	samples_total = data->bytes_total / (data->bits_per_sample / 8);
	playtime = (gdouble) samples_total / data->samplerate / data->channels;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		xmms_xform_metadata_set_int (xform, metakey, playtime * 1000);
	}

	bitrate = data->bits_per_sample * data->samplerate / data->channels;
	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
	xmms_xform_metadata_set_int (xform, metakey, bitrate);
}

static gboolean
xmms_wave_init (xmms_xform_t *xform)
{
	xmms_wave_data_t *data;
	xmms_error_t error;
	xmms_sample_format_t sample_fmt;
	xmms_wave_format_t fmt;
	guint8 buf[1024];
	gint read;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_wave_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_xform_private_data_set (xform, data);

	read = xmms_xform_peek (xform, (gchar *) buf, sizeof (buf), &error);

	if (read < WAVE_HEADER_MIN_SIZE) {
		xmms_log_error ("Could not read wave header");
		return FALSE;
	}

	fmt = read_wave_header (data, buf, read);

	switch (fmt) {
		case WAVE_FORMAT_UNDEFINED:
			xmms_log_error ("Not a valid Wave stream");
			return FALSE;
		case WAVE_FORMAT_MP3:
			xmms_xform_outdata_type_add (xform,
			                             XMMS_STREAM_TYPE_MIMETYPE,
			                             "audio/mpeg",
			                             XMMS_STREAM_TYPE_END);
			break;
		case WAVE_FORMAT_PCM:
			xmms_wave_get_media_info (xform);

			if (read < data->header_size) {
				xmms_log_info ("Wave header too big?");
				return FALSE;
			}
			/* skip over the header */
			xmms_xform_read (xform, (gchar *) buf, data->header_size, &error);

			sample_fmt = (data->bits_per_sample == 8 ? XMMS_SAMPLE_FORMAT_U8
			                                         : XMMS_SAMPLE_FORMAT_S16);

			xmms_xform_outdata_type_add (xform,
			                             XMMS_STREAM_TYPE_MIMETYPE,
			                             "audio/pcm",
			                             XMMS_STREAM_TYPE_FMT_FORMAT,
			                             sample_fmt,
			                             XMMS_STREAM_TYPE_FMT_CHANNELS,
			                             data->channels,
			                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
			                             data->samplerate,
			                             XMMS_STREAM_TYPE_END);
	}

	return TRUE;
}

static gint
xmms_wave_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                xmms_error_t *error)
{
	xmms_wave_data_t *data;
	gint ret;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = xmms_xform_read (xform, (gchar *) buf, len, error);
	if (ret <= 0) {
		return ret;
	}

#if G_BYTE_ORDER == G_BIG_ENDIAN
	if (data->bits_per_sample == 16) {
		gint16 *s = (gint16 *) buf;
		gint i;

		for (i = 0; i < (ret / 2); i++) {
			s[i] = GINT16_FROM_LE (s[i]);
		}
	}
#endif

	return ret;
}

static gint64
xmms_wave_seek (xmms_xform_t *xform, gint64 samples,
                xmms_xform_seek_mode_t whence, xmms_error_t *error)
{
	xmms_wave_data_t *data;
	gint64 offset;
	gint64 ret;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (samples >= 0, -1);
	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	/* in mp3 mode, the samples argument actually indicates bytes..
	 * thus we've set up bits_per_sample to 8 and channels to 1 to get
	 * expected behaviour. */

	offset = data->header_size;
	offset += samples * (data->bits_per_sample / 8) * data->channels;

	if (offset < data->header_size) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
		                "Trying to seek before start of stream");
		return -1;
	}

	if (offset > data->header_size + data->bytes_total) {
		xmms_error_set (error, XMMS_ERROR_INVAL,
		                "Trying to seek past end of stream");
		return -1;
	}

	ret = xmms_xform_seek (xform, offset, whence, error);

	if (ret == -1) {
		return -1;
	}

	if (ret != offset) {
		XMMS_DBG ("xmms_xform_seek didn't return expected offset "
		          "(%" G_GINT64_FORMAT " != %" G_GINT64_FORMAT ")",
		          ret, offset);
	}

	ret -= data->header_size;

	ret /= (data->bits_per_sample / 8) * data->channels;

	return ret;
}

static void
xmms_wave_destroy (xmms_xform_t *xform)
{
	g_return_if_fail (xform);

	g_free (xmms_xform_private_data_get (xform));
}

static xmms_wave_format_t
read_wave_header (xmms_wave_data_t *data, guint8 *buf, gint bytes_read)
{
	gchar stmp[5];
	guint32 tmp32, data_size;
	guint16 tmp16;
	gint bytes_left = bytes_read;
	xmms_wave_format_t ret = WAVE_FORMAT_UNDEFINED;

	if (bytes_left < WAVE_HEADER_MIN_SIZE) {
		xmms_log_error ("Not enough data for wave header");
		return ret;
	}

	GET_STR (buf, stmp, 4);
	if (strcmp (stmp, "RIFF")) {
		xmms_log_error ("No RIFF data");
		return ret;
	}

	GET_32 (buf, data_size);
	data_size += 8;

	GET_STR (buf, stmp, 4);
	if (strcmp (stmp, "WAVE")) {
		xmms_log_error ("No Wave data");
		return ret;
	}

	GET_STR (buf, stmp, 4);
	if (strcmp (stmp, "fmt ")) {
		xmms_log_error ("Format chunk missing");
		return ret;
	}

	GET_32 (buf, tmp32); /* format chunk length */
	XMMS_DBG ("format chunk length: %i", tmp32);

	GET_16 (buf, tmp16); /* format tag */
	ret = tmp16;

	switch (tmp16) {
		case WAVE_FORMAT_PCM:
			if (tmp32 != 16) {
				xmms_log_error ("Format chunk length not 16.");
				return WAVE_FORMAT_UNDEFINED;
			}

			GET_16 (buf, data->channels);
			XMMS_DBG ("channels %i", data->channels);
			if (data->channels < 1 || data->channels > 2) {
				xmms_log_error ("Unhandled number of channels: %i",
				                data->channels);
				return WAVE_FORMAT_UNDEFINED;
			}

			GET_32 (buf, data->samplerate);
			XMMS_DBG ("samplerate %i", data->samplerate);
			if (data->samplerate != 8000 && data->samplerate != 11025 &&
			    data->samplerate != 22050 && data->samplerate != 44100) {
				xmms_log_error ("Invalid samplerate: %i", data->samplerate);
				return WAVE_FORMAT_UNDEFINED;
			}

			GET_32 (buf, tmp32);
			GET_16 (buf, tmp16);

			GET_16 (buf, data->bits_per_sample);
			XMMS_DBG ("bits per sample %i", data->bits_per_sample);
			if (data->bits_per_sample != 8 && data->bits_per_sample != 16) {
				xmms_log_error ("Unhandled bits per sample: %i",
				                data->bits_per_sample);
				return WAVE_FORMAT_UNDEFINED;
			}

			break;
		case WAVE_FORMAT_MP3:
			SKIP (tmp32 - sizeof (tmp16));
			/* set up so that seeking works with "bytes" as seek unit */
			data->bits_per_sample = 8;
			data->channels = 1;
			break;
		default:
			xmms_log_error ("unhandled format tag: 0x%x", tmp16);
			return WAVE_FORMAT_UNDEFINED;
	}

	GET_STR (buf, stmp, 4);

	while (strcmp (stmp, "data")) {
		GET_32 (buf, tmp32);

		if (bytes_left < (tmp32 + 8)) {
			xmms_log_error ("Data chunk missing");
			return WAVE_FORMAT_UNDEFINED;
		}

		buf += tmp32;
		bytes_left -= tmp32;

		GET_STR (buf, stmp, 4);
	}

	GET_32 (buf, data->bytes_total);

	data->header_size = bytes_read - bytes_left;

	if (data->bytes_total + data->header_size != data_size) {
		xmms_log_info ("Data chunk size doesn't match RIFF chunk size");
		/* don't return FALSE here, we try to read it anyway */
	}

	return ret;
}
