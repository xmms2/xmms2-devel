/** @file avcodec.c
 *  Decoder plugin for ffmpeg avcodec formats
 *
 *  Copyright (C) 2006-2023 XMMS2 Team
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

#include <xmms_configuration.h>
#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libavutil/mem.h>

#include "avcodec_compat.h"

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(60,0,0) /* ffmpeg < 7 */
#    define XMMS2_AVCODEC_CHANNEL_FIELD(codecctx) (codecctx)->channels
#else
#    define XMMS2_AVCODEC_CHANNEL_FIELD(codecctx) (codecctx)->ch_layout.nb_channels
#endif


#define AVCODEC_BUFFER_SIZE 16384

typedef struct {
	AVCodecContext *codecctx;
	AVPacket packet;

	guchar *buffer;
	guint buffer_length;
	guint buffer_size;
	gboolean no_demuxer;

	AVFrame *read_out_frame;

	guint channels;
	guint samplerate;
	xmms_sample_format_t sampleformat;

	gint bitrate;
	gint samplebits;
	gint block_align;
	const gchar *codec_id;
	gpointer extradata;
	gssize extradata_size;

	GString *outbuf;
} xmms_avcodec_data_t;

static gboolean xmms_avcodec_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_avcodec_init (xmms_xform_t *xform);
static void xmms_avcodec_destroy (xmms_xform_t *xform);
static gint xmms_avcodec_internal_read_some (xmms_xform_t *xform, xmms_avcodec_data_t *data, xmms_error_t *error);
static gint xmms_avcodec_internal_decode_some (xmms_avcodec_data_t *data);
static void xmms_avcodec_internal_append (xmms_avcodec_data_t *data);
static gint xmms_avcodec_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                               xmms_error_t *error);
static gint64 xmms_avcodec_seek (xmms_xform_t *xform, gint64 samples,
                                 xmms_xform_seek_mode_t whence, xmms_error_t *err);
static xmms_sample_format_t xmms_avcodec_translate_sample_format (enum AVSampleFormat av_sample_format);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("avcodec",
                          "AVCodec Decoder", XMMS_VERSION,
                          "ffmpeg libavcodec decoder",
                          xmms_avcodec_plugin_setup);

static gboolean
xmms_avcodec_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_avcodec_init;
	methods.destroy = xmms_avcodec_destroy;
	methods.read = xmms_avcodec_read;
	methods.seek = xmms_avcodec_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_magic_add ("Shorten header", "audio/x-ffmpeg-shorten",
	                "0 string ajkg", NULL);
	xmms_magic_add ("A/52 (AC-3) header", "audio/x-ffmpeg-ac3",
	                "0 beshort 0x0b77", NULL);
	xmms_magic_add ("DTS header", "audio/x-ffmpeg-dca",
	                "0 belong 0x7ffe8001", NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-ffmpeg-*",
	                              XMMS_STREAM_TYPE_END);

	XMMS_DBG ("avcodec version at build time is %d.%d.%d",
	          (LIBAVCODEC_VERSION_INT >> 16),
	          (LIBAVCODEC_VERSION_INT >> 8) & 0xff,
	          LIBAVCODEC_VERSION_INT & 0xff);
	XMMS_DBG ("avcodec version at run time is %d.%d.%d",
	          (avcodec_version() >> 16),
	          (avcodec_version() >> 8) & 0xff,
	          avcodec_version() & 0xff);
	XMMS_DBG ("avcodec configuration is %s", avcodec_configuration());

	return TRUE;
}

static void
xmms_avcodec_destroy (xmms_xform_t *xform)
{
	xmms_avcodec_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	avcodec_free_context (&(data->codecctx));
	av_frame_free (&data->read_out_frame);

	g_string_free (data->outbuf, TRUE);
	g_free (data->buffer);
	g_free (data->extradata);
	g_free (data);
}

static gboolean
xmms_avcodec_init (xmms_xform_t *xform)
{
	xmms_avcodec_data_t *data;
	const AVCodec *codec;
	const gchar *mimetype;
	const guchar *tmpbuf;
	gsize tmpbuflen;
	gint ret;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_avcodec_data_t, 1);
	data->outbuf = g_string_new (NULL);
	data->buffer = g_malloc (AVCODEC_BUFFER_SIZE);
	data->buffer_size = AVCODEC_BUFFER_SIZE;
	data->codecctx = NULL;
	data->packet.size = 0;

	data->read_out_frame = av_frame_alloc ();

	xmms_xform_private_data_set (xform, data);

	mimetype = xmms_xform_indata_get_str (xform,
	                                      XMMS_STREAM_TYPE_MIMETYPE);
	data->codec_id = mimetype + strlen ("audio/x-ffmpeg-");

	codec = avcodec_find_decoder_by_name (data->codec_id);
	if (!codec) {
		XMMS_DBG ("No supported decoder with name '%s' found", data->codec_id);
		goto err;
	}

	if (codec->type != AVMEDIA_TYPE_AUDIO) {
		XMMS_DBG ("Codec '%s' found but its type is not audio", data->codec_id);
		goto err;
	}

	ret = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_SAMPLERATE);
	if (ret > 0) {
		data->samplerate = ret;
	}
	ret = xmms_xform_indata_get_int (xform, XMMS_STREAM_TYPE_FMT_CHANNELS);
	if (ret > 0) {
		data->channels = ret;
	}

	/* Required by WMA xform. */
	xmms_xform_auxdata_get_int (xform,
	                            "bitrate",
	                            &data->bitrate);

	/* Required by tta and apefile xforms. */
	xmms_xform_auxdata_get_int (xform,
	                            "samplebits",
	                            &data->samplebits);

	xmms_xform_auxdata_get_int (xform,
	                            "block_align",
	                            &data->block_align);

	ret = xmms_xform_auxdata_get_bin (xform, "decoder_config",
	                                  &tmpbuf, &tmpbuflen);

	if (ret) {
		data->extradata = g_memdup (tmpbuf, tmpbuflen);
		data->extradata_size = tmpbuflen;
	} else {
		/* This should be a list of known formats that don't have a
		 * demuxer so they will be handled slightly differently... */
		if (!strcmp (data->codec_id, "shorten") ||
		    !strcmp (data->codec_id, "adpcm_swf") ||
		    !strcmp (data->codec_id, "pcm_s16le") ||
		    !strcmp (data->codec_id, "ac3") ||
		    !strcmp (data->codec_id, "dca") ||
		    !strcmp (data->codec_id, "nellymoser")) {
			/* number 1024 taken from libavformat raw.c RAW_PACKET_SIZE */
			data->extradata = g_malloc0 (1024);
			data->extradata_size = 1024;
			data->no_demuxer = TRUE;
		} else {
			/* A demuxer plugin forgot to give decoder config? */
			xmms_log_error ("Decoder config data not found!");
			goto err;
		}
	}

	data->codecctx = avcodec_alloc_context3 (codec);
	data->codecctx->sample_rate = data->samplerate;
	XMMS2_AVCODEC_CHANNEL_FIELD(data->codecctx) = data->channels;
	data->codecctx->bit_rate = data->bitrate;
	data->codecctx->bits_per_coded_sample = data->samplebits;
	data->codecctx->block_align = data->block_align;
	data->codecctx->extradata = data->extradata;
	data->codecctx->extradata_size = data->extradata_size;
	data->codecctx->codec_id = codec->id;
	data->codecctx->codec_type = codec->type;

	if (avcodec_open2 (data->codecctx, codec, NULL) < 0) {
		XMMS_DBG ("Opening decoder '%s' failed", codec->name);
		goto err;
	} else {
		gchar buf[42];
		xmms_error_t error;

		/* some codecs need to have something read before they set
		 * the samplerate and channels correctly, unfortunately... */
		if ((ret = xmms_avcodec_read (xform, buf, sizeof (buf), &error)) > 0) {
			g_string_insert_len (data->outbuf, 0, buf, ret);
		} else {
			XMMS_DBG ("First read failed, codec is not working...");
			goto err;
		}
	}

	data->samplerate = data->codecctx->sample_rate;
	data->channels = XMMS2_AVCODEC_CHANNEL_FIELD(data->codecctx);
	data->sampleformat = xmms_avcodec_translate_sample_format (data->codecctx->sample_fmt);
	if (data->sampleformat == XMMS_SAMPLE_FORMAT_UNKNOWN) {
		goto err;
	}

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             data->sampleformat,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->samplerate,
	                             XMMS_STREAM_TYPE_END);

	XMMS_DBG ("Decoder %s at rate %d with %d channels of format %s initialized",
	          codec->name, data->codecctx->sample_rate,
	          XMMS2_AVCODEC_CHANNEL_FIELD(data->codecctx),
	          av_get_sample_fmt_name (data->codecctx->sample_fmt));

	return TRUE;

err:
	if (data->codecctx) {
		avcodec_free_context (&(data->codecctx));
	}
	if (data->read_out_frame) {
		avcodec_free_frame (&data->read_out_frame);
	}
	g_string_free (data->outbuf, TRUE);
	g_free (data->extradata);
	g_free (data);

	return FALSE;
}

static gint
xmms_avcodec_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                   xmms_error_t *error)
{
	xmms_avcodec_data_t *data;
	guint size;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	while (0 == (size = MIN (data->outbuf->len, len))) {
		gint res;

		if (data->no_demuxer || data->buffer_length == 0) {
			gint bytes_read;

			bytes_read = xmms_avcodec_internal_read_some (xform, data, error);
			if (bytes_read <= 0) { return bytes_read; }
		}

		res = xmms_avcodec_internal_decode_some (data);
		if (res < 0) { return res; }
		if (res > 0) { xmms_avcodec_internal_append (data); }
	}

	memcpy (buf, data->outbuf->str, size);
	g_string_erase (data->outbuf, 0, size);

	return size;
}

static gint64
xmms_avcodec_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_avcodec_data_t *data;
	gint64 ret = -1;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	/* We can't seek without a demuxer in general case */
	if (data->no_demuxer) {
		xmms_error_set (err, XMMS_ERROR_GENERIC,
		                "Can't seek in avcodec plugin without a demuxer!");
		return -1;
	}

	/* The buggy ape decoder doesn't flush buffers, so we need to finish decoding
	 * the frame before seeking to avoid segfaults... this hack sucks */
	/* FIXME: Is ^^^ still true? */
	while (data->buffer_length > 0) {
		if (xmms_avcodec_internal_decode_some (data) < 0) {
			return -1;
		}
	}

	ret = xmms_xform_seek (xform, samples, whence, err);

	if (ret >= 0) {
		avcodec_flush_buffers (data->codecctx);

		data->buffer_length = 0;
		g_string_erase (data->outbuf, 0, -1);
	}

	return ret;
}

static xmms_sample_format_t
xmms_avcodec_translate_sample_format (enum AVSampleFormat av_sample_format)
{
	switch (av_sample_format) {
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_U8P:
		return XMMS_SAMPLE_FORMAT_U8;
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		return XMMS_SAMPLE_FORMAT_S16;
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_S32P:
		return XMMS_SAMPLE_FORMAT_S32;
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_FLTP:
		return XMMS_SAMPLE_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_DBL:
	case AV_SAMPLE_FMT_DBLP:
		return XMMS_SAMPLE_FORMAT_DOUBLE;
	default:
		XMMS_DBG ("AVSampleFormat (%i: %s) not supported.", av_sample_format,
		          av_get_sample_fmt_name (av_sample_format));
		return XMMS_SAMPLE_FORMAT_UNKNOWN;
	}
}

/*
Read some data from our source of data to data->buffer, updating buffer_length
and buffer_size as needed.

Returns: on error: negative
         on EOF: zero
         otherwise: number of bytes read.
*/
static gint
xmms_avcodec_internal_read_some (xmms_xform_t *xform,
                                 xmms_avcodec_data_t *data,
                                 xmms_error_t *error)
{
	gint bytes_read, read_total;

	bytes_read = xmms_xform_read (xform,
	                              (gchar *) (data->buffer + data->buffer_length),
	                              data->buffer_size - data->buffer_length,
	                              error);

	if (bytes_read < 0) {
		XMMS_DBG ("Error while reading data");
		return bytes_read;
	} else if (bytes_read == 0) {
		XMMS_DBG ("EOF");
		return 0;
	}

	read_total = bytes_read;

	/* If we have a demuxer plugin, make sure we read the whole packet */
	while (read_total == data->buffer_size && !data->no_demuxer) {
		/* multiply the buffer size and try to read again */
		data->buffer = g_realloc (data->buffer, data->buffer_size * 2);
		bytes_read = xmms_xform_read (xform,
		                              (gchar *) data->buffer +
		                                data->buffer_size,
		                              data->buffer_size,
		                              error);
		data->buffer_size *= 2;

		if (bytes_read < 0) {
			XMMS_DBG ("Error while reading data");
			return bytes_read;
		}

		read_total += bytes_read;

		if (read_total < data->buffer_size) {
			/* finally double the buffer size for performance reasons, the
			 * hotspot handling likes to fit two frames in the buffer */
			data->buffer = g_realloc (data->buffer, data->buffer_size * 2);
			data->buffer_size *= 2;
			XMMS_DBG ("Reallocated avcodec internal buffer to be %d bytes",
			          data->buffer_size);

			break;
		}
	}

	/* Update the buffer length */
	data->buffer_length += read_total;

	return read_total;
}

/*
Decode some data from data->buffer[0..data->buffer_length-1] to
data->read_out_frame

Returns: on error: negative
         on no new data produced: zero
         otherwise: positive

FIXME: data->buffer should be at least data->buffer_length +
FF_INPUT_BUFFER_PADDING_SIZE long.
*/
static gint
xmms_avcodec_internal_decode_some (xmms_avcodec_data_t *data)
{
	int rc = 0;

	if (data->packet.size == 0) {
		av_init_packet (&data->packet);
		data->packet.data = data->buffer;
		data->packet.size = data->buffer_length;

		rc = avcodec_send_packet(data->codecctx, &data->packet);
		if (rc == AVERROR_EOF)
			rc = 0;
	}

	if (rc == 0) {
		rc = avcodec_receive_frame(data->codecctx, data->read_out_frame);
		if (rc < 0) {
			data->packet.size = 0;
			data->buffer_length = 0;
			if (rc == AVERROR(EAGAIN)) rc = 0;
			else if (rc == AVERROR_EOF) rc = 1;
		}
		else
			rc = 1;
	}

	if (rc < 0) {
		data->packet.size = 0;
		XMMS_DBG ("Error decoding data!");
		return -1;
	}

	return rc;
}

static void
xmms_avcodec_internal_append (xmms_avcodec_data_t *data)
{
	enum AVSampleFormat fmt = (enum AVSampleFormat) data->read_out_frame->format;
	int samples = data->read_out_frame->nb_samples;
	int channels = XMMS2_AVCODEC_CHANNEL_FIELD(data->codecctx);
	int bps = av_get_bytes_per_sample (fmt);

	if (av_sample_fmt_is_planar (fmt)) {
		/* Convert from planar to packed format */
		gint i, j;

		for (i = 0; i < samples; i++) {
			for (j = 0; j < channels; j++) {
				g_string_append_len (
					data->outbuf,
					(gchar *) (data->read_out_frame->extended_data[j] + i*bps),
					bps
				);
			}
		}
	} else {
		g_string_append_len (data->outbuf,
		                     (gchar *) data->read_out_frame->extended_data[0],
		                     samples * channels * bps);
	}
}
