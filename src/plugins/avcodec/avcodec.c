/** @file avcodec.c
 *  Decoder plugin for ffmpeg avcodec formats
 *
 *  Copyright (C) 2006-2009 XMMS2 Team
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

#include "xmms_configuration.h"
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#undef ABS
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
# include "libavcodec/avcodec.h"
#else
# include "avcodec.h"
#endif

/* Map avcodec_decode_audio2 into the deprecated version
 * avcodec_decode_audio in versions earlier than 51.28 */
#if LIBAVCODEC_VERSION_INT < 0x331c00
# define avcodec_decode_audio2 avcodec_decode_audio
#endif

/* Handle API change that happened in libavcodec 52.00 */
#if LIBAVCODEC_VERSION_INT < 0x340000
# define CONTEXT_BPS(codecctx) (codecctx)->bits_per_sample
#else
# define CONTEXT_BPS(codecctx) (codecctx)->bits_per_coded_sample
#endif

/* Map avcodec_decode_audio3 into the deprecated version
 * avcodec_decode_audio2 in versions earlier than 52.26 */
#if LIBAVCODEC_VERSION_INT < 0x341a00
# define avcodec_decode_audio3(avctx, samples, frame_size_ptr, avpkt) \
    avcodec_decode_audio2(avctx, samples, frame_size_ptr, \
                          (avpkt)->data, (avpkt)->size)
# define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif

/* Handle API change that happened in libavcodec 52.64 */
#if LIBAVCODEC_VERSION_INT < 0x344000
# define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif

#define AVCODEC_BUFFER_SIZE 16384

typedef struct {
	AVCodecContext *codecctx;

	guchar *buffer;
	guint buffer_length;
	guint buffer_size;
	gboolean no_demuxer;

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
static gint xmms_avcodec_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                               xmms_error_t *error);
static gint64 xmms_avcodec_seek (xmms_xform_t *xform, gint64 samples,
                                 xmms_xform_seek_mode_t whence, xmms_error_t *err);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("avcodec",
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
	                              NULL);

	return TRUE;
}

static void
xmms_avcodec_destroy (xmms_xform_t *xform)
{
	xmms_avcodec_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	avcodec_close (data->codecctx);
	av_free (data->codecctx);

	g_string_free (data->outbuf, TRUE);
	g_free (data->buffer);
	g_free (data->extradata);
	g_free (data);
}

static gboolean
xmms_avcodec_init (xmms_xform_t *xform)
{
	xmms_avcodec_data_t *data;
	AVCodec *codec;
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

	xmms_xform_private_data_set (xform, data);

	avcodec_init ();
	avcodec_register_all ();

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

	/* bitrate required for WMA files */
	xmms_xform_auxdata_get_int (xform,
	                            "bitrate",
	                            &data->bitrate);

	/* ALAC and MAC require bits per sample field to be 16 */
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
		    !strcmp (data->codec_id, "dca")) {
			/* number 1024 taken from libavformat raw.c RAW_PACKET_SIZE */
			data->extradata = g_malloc0 (1024);
			data->extradata_size = 1024;
			data->no_demuxer = TRUE;
		} else {
			/* A demuxer plugin forgot to give decoder config? */
			xmms_log_error ("Decoder config data not found!");
			return FALSE;
		}
	}

	data->codecctx = avcodec_alloc_context ();
	data->codecctx->sample_rate = data->samplerate;
	data->codecctx->channels = data->channels;
	data->codecctx->bit_rate = data->bitrate;
	CONTEXT_BPS (data->codecctx) = data->samplebits;
	data->codecctx->block_align = data->block_align;
	data->codecctx->extradata = data->extradata;
	data->codecctx->extradata_size = data->extradata_size;
	data->codecctx->codec_id = codec->id;
	data->codecctx->codec_type = codec->type;

	if (avcodec_open (data->codecctx, codec) < 0) {
		XMMS_DBG ("Opening decoder '%s' failed", codec->name);
		goto err;
	} else {
		gchar buf[42];
		xmms_error_t error;

		/* some codecs need to have something read before they set
		 * the samplerate and channels correctly, unfortunately... */
		if ((ret = xmms_avcodec_read (xform, buf, 42, &error)) > 0) {
			g_string_insert_len (data->outbuf, 0, buf, ret);
		} else {
			XMMS_DBG ("First read failed, codec is not working...");
			avcodec_close (data->codecctx);
			goto err;
		}
	}

	data->samplerate = data->codecctx->sample_rate;
	data->channels = data->codecctx->channels;

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->samplerate,
	                             XMMS_STREAM_TYPE_END);

	XMMS_DBG ("Decoder '%s' initialized successfully!", codec->name);

	return TRUE;

err:
	if (data->codecctx) {
		av_free (data->codecctx);
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
	char outbuf[AVCODEC_MAX_AUDIO_FRAME_SIZE];
	gint outbufsize, bytes_read = 0;
	guint size;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	size = MIN (data->outbuf->len, len);
	while (size == 0) {
		AVPacket packet;
		av_init_packet (&packet);

		if (data->no_demuxer || data->buffer_length == 0) {
			gint read_total;

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
		}

		packet.data = data->buffer;
		packet.size = data->buffer_length;

		outbufsize = sizeof (outbuf);
		bytes_read = avcodec_decode_audio3 (data->codecctx, (short *) outbuf,
		                                    &outbufsize, &packet);

		/* The DTS decoder of ffmpeg is buggy and always returns
		 * the input buffer length, get frame length from header */
		if (!strcmp (data->codec_id, "dca") && bytes_read > 0) {
			bytes_read = ((int)data->buffer[5] << 12) |
			             ((int)data->buffer[6] << 4) |
			             ((int)data->buffer[7] >> 4);
			bytes_read = (bytes_read & 0x3fff) + 1;
		}

		if (bytes_read < 0 || bytes_read > data->buffer_length) {
			XMMS_DBG ("Error decoding data!");
			return -1;
		} else if (bytes_read != data->buffer_length) {
			g_memmove (data->buffer,
			           data->buffer + bytes_read,
			           data->buffer_length - bytes_read);
			data->buffer_length -= bytes_read;
		}

		if (outbufsize > 0) {
			g_string_append_len (data->outbuf, outbuf, outbufsize);
		}

		size = MIN (data->outbuf->len, len);
	}

	memcpy (buf, data->outbuf->str, size);
	g_string_erase (data->outbuf, 0, size);

	return size;
}

static gint64
xmms_avcodec_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_avcodec_data_t *data;
	char outbuf[AVCODEC_MAX_AUDIO_FRAME_SIZE];
	gint outbufsize, bytes_read = 0;
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
	while (data->buffer_length > 0) {
		AVPacket packet;
		av_init_packet (&packet);
		packet.data = data->buffer;
		packet.size = data->buffer_length;

		outbufsize = sizeof (outbuf);
		bytes_read = avcodec_decode_audio3 (data->codecctx, (short *) outbuf,
		                                    &outbufsize, &packet);

		if (bytes_read < 0 || bytes_read > data->buffer_length) {
			XMMS_DBG ("Error decoding data!");
			return -1;
		}

		data->buffer_length -= bytes_read;
		g_memmove (data->buffer, data->buffer + bytes_read, data->buffer_length);
	}

	ret = xmms_xform_seek (xform, samples, whence, err);

	if (ret >= 0) {
		avcodec_flush_buffers (data->codecctx);

		data->buffer_length = 0;
		g_string_erase (data->outbuf, 0, -1);
	}

	return ret;
}
