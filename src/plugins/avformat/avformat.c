/** @file avformat.c
 *  Avformat decoder plugin
 *
 *  Copyright (C) 2006-2007 XMMS2 Team
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
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#undef ABS
#include "avformat.h"

#define AVFORMAT_BUFFER_SIZE 4096

typedef struct {
	gint track;

	AVFormatContext *fmtctx;
	AVCodecContext *codecctx;
	offset_t offset;

	guchar buffer[AVFORMAT_BUFFER_SIZE];
	guint buffer_size;

	GString *outbuf;
} xmms_avformat_data_t;

static gboolean xmms_avformat_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_avformat_init (xmms_xform_t *xform);
static void xmms_avformat_destroy (xmms_xform_t *xform);
static gint xmms_avformat_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                                xmms_error_t *err);
static void xmms_avformat_get_mediainfo (xmms_xform_t *xform);
int xmms_avformat_get_track (AVFormatContext *fmtctx);

int xmms_avformat_read_callback (void *user_data, uint8_t *buffer,
                                 int length);
offset_t xmms_avformat_seek_callback (void *user_data, offset_t offset, int whence);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("avformat",
                   "AVFormat Demuxer", XMMS_VERSION,
                   "ffmpeg libavformat demuxer",
                   xmms_avformat_plugin_setup);

static gboolean
xmms_avformat_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_avformat_init;
	methods.destroy = xmms_avformat_destroy;
	methods.read = xmms_avformat_read;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/3gpp",
	                              NULL);

	xmms_magic_add ("amr header", "audio/3gpp",
	                "0 string #!AMR", NULL);

	return TRUE;
}

static void
xmms_avformat_destroy (xmms_xform_t *xform)
{
	xmms_avformat_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	av_close_input_file (data->fmtctx);

	g_string_free (data->outbuf, TRUE);
	g_free (data);
}

static gboolean
xmms_avformat_init (xmms_xform_t *xform)
{
	xmms_avformat_data_t *data;
	AVInputFormat *format;
	ByteIOContext byteio;
	AVCodec *codec;
	const gchar *mimetype;
	gint temp;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_avformat_data_t, 1);
	data->outbuf = g_string_new (NULL);
	data->buffer_size = AVFORMAT_BUFFER_SIZE;
	data->offset = 0;

	xmms_xform_private_data_set (xform, data);

	av_register_all ();

	mimetype = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_MIMETYPE);
	if (!strcmp (mimetype, "audio/3gpp")) {
		format = av_find_input_format ("amr");
		if (!format) {
			XMMS_DBG ("AMR format not registered to library");
			goto err;
		}
	} else {
		/* Unknown mimetype, init failed */
		return FALSE;
	}

	format->flags |= AVFMT_NOFILE;
	if ((temp = init_put_byte (&byteio, data->buffer, data->buffer_size, 0,
	                           xform, xmms_avformat_read_callback, NULL,
	                           xmms_avformat_seek_callback)) < 0) {
		XMMS_DBG ("Could not initialize ByteIOContext structure: %d", temp);
		goto err;
	}
	if ((temp = av_open_input_stream (&data->fmtctx, &byteio, "", format,
	                                  NULL)) < 0) {
		XMMS_DBG ("Could not open input stream for ASF format: %d", temp);
		goto err;
	}
	data->track = xmms_avformat_get_track (data->fmtctx);
	if (data->track < 0) {
		XMMS_DBG ("Could not find WMA data track from file");
		goto err;
	}

	data->codecctx = data->fmtctx->streams[data->track]->codec;
	codec = avcodec_find_decoder (data->codecctx->codec_id);

	if (!strcmp (codec->name, "wmav1")) {
		mimetype = "audio/x-ffmpeg-wmav1";
	} else if (!strcmp (codec->name, "wmav2")) {
		mimetype = "audio/x-ffmpeg-wmav2";
	} else if (!strcmp (codec->name, "amr_nb")) {
		mimetype = "audio/x-ffmpeg-amr_nb";
	} else if (!strcmp (codec->name, "amr_wb")) {
		mimetype = "audio/x-ffmpeg-amr_wb";
	} else {
		xmms_log_error ("Unknown codec %s inside asf stream",
		                codec->name);
		goto err;
	}

	XMMS_DBG ("mimetype set to '%s'", mimetype);

	xmms_xform_privdata_set_bin (xform,
	                             "decoder_config",
	                             data->codecctx->extradata,
	                             data->codecctx->extradata_size);

	xmms_avformat_get_mediainfo (xform);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             mimetype,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->codecctx->sample_rate,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->codecctx->channels,
	                             XMMS_STREAM_TYPE_END);

	XMMS_DBG ("ASF demuxer inited successfully!");

	return TRUE;

err:
	if (data->fmtctx) {
		av_close_input_file (data->fmtctx);
	}
	g_string_free (data->outbuf, TRUE);
	g_free (data);

	return FALSE;
}

static gint
xmms_avformat_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                    xmms_error_t *err)
{
	xmms_avformat_data_t *data;
	AVPacket pkt;
	int size;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	size = MIN (data->outbuf->len, len);
	while (size == 0) {
		if (av_read_frame (data->fmtctx, &pkt) < 0)
			return -1;
		if (pkt.size == 0)
			return 0;

		xmms_xform_privdata_set_none (xform);

		g_string_append_len (data->outbuf, (gchar *) pkt.data, pkt.size);

		if (pkt.data) {
			av_free_packet (&pkt);
		}

		size = MIN (data->outbuf->len, len);
	}

	memcpy (buf, data->outbuf->str, size);
	g_string_erase (data->outbuf, 0, size);

	return size;
}

static void
xmms_avformat_get_mediainfo (xmms_xform_t *xform)
{
	xmms_avformat_data_t *data;
	AVFormatContext *fmtctx;
	AVCodecContext *codecctx;
	const gchar *metakey;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	fmtctx = data->fmtctx;
	codecctx = data->codecctx;

	if (codecctx->bit_rate > 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
		xmms_xform_metadata_set_int (xform, metakey, codecctx->bit_rate);
	}
	if (fmtctx->streams[data->track]->duration > 0) {
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		xmms_xform_metadata_set_int (xform, metakey,
		                             fmtctx->streams[data->track]->duration);
	}
}

int
xmms_avformat_read_callback (void *user_data, uint8_t *buffer, int length)
{
	xmms_xform_t *xform;
	xmms_avformat_data_t *data;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (user_data, 0);
	g_return_val_if_fail (buffer, 0);
	xform = user_data;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	ret = xmms_xform_read (xform, buffer, length, &error);

	if (ret > 0) {
		data->offset += ret;
	}

	return ret;
}

offset_t
xmms_avformat_seek_callback (void *user_data, offset_t offset, int whence)
{
	xmms_xform_t *xform;
	xmms_avformat_data_t *data;
	xmms_error_t error;
	gint64 ret;
	gint w;

	g_return_val_if_fail (user_data, -1);
	xform = user_data;

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	if (whence == SEEK_END) {
		w = XMMS_XFORM_SEEK_END;
	} else if (whence == SEEK_CUR) {
		w = XMMS_XFORM_SEEK_CUR;
	} else {
		/* change SEEK_SET to SEEK_CUR */
		w = XMMS_XFORM_SEEK_CUR;
		offset -= data->offset;
	}

	if (w == XMMS_XFORM_SEEK_CUR && offset <= 4096) {
		guint8 buf[4096];

		ret = data->offset + xmms_xform_read (xform, buf, offset, &error);
	} else {
		ret = xmms_xform_seek (xform, offset, w, &error);
	}

	if (ret > 0) {
		data->offset = ret;
	}

	return ret;
}

int
xmms_avformat_get_track (AVFormatContext *fmtctx)
{
	gint wma_idx;
	AVCodecContext *codec;

	g_return_val_if_fail (fmtctx, -1);

	for (wma_idx = 0; wma_idx < fmtctx->nb_streams; wma_idx++) {
		codec = fmtctx->streams[wma_idx]->codec;

		if (codec->codec_type == CODEC_TYPE_AUDIO) {
			break;
		}
	}
	if (wma_idx == fmtctx->nb_streams) {
		return -1;
	}

	return wma_idx;
}

