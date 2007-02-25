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

typedef struct {
	guint32 v1;
	guint16 v2;
	guint16 v3;
	guint8 v4[8];
} xmms_asf_GUID_t;

xmms_asf_GUID_t xmms_asf_GUIDs[] = {
	/* header */
	{ 0x75B22630, 0x668E, 0x11CF,
	  { 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C } },
	/* content description */
	{ 0x75b22633, 0x668e, 0x11cf,
	  { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c } },
	/* extended content description */
	{ 0xD2D0A440, 0xE307, 0x11D2,
	  { 0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50 } },
};

static gboolean xmms_avformat_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_avformat_init (xmms_xform_t *xform);
static void xmms_avformat_destroy (xmms_xform_t *xform);
static gint xmms_avformat_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                                xmms_error_t *err);
static void xmms_avformat_get_mediainfo (xmms_xform_t *xform);
int xmms_avformat_get_track (AVFormatContext *fmtctx);

static void xmms_asf_metahack (xmms_xform_t *xform);

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
	                              "video/x-ms-asf",
	                              NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/3gpp",
	                              NULL);

	xmms_magic_add ("asf header", "video/x-ms-asf",
	                "0 belong 0x3026b275", NULL);

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
	if (!strcmp (mimetype, "video/x-ms-asf")) {
		xmms_asf_metahack (xform);

		format = av_find_input_format ("asf");
		if (!format) {
			XMMS_DBG ("ASF format not registered to library");
			goto err;
		}
	} else if (!strcmp (mimetype, "audio/3gpp")) {
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
xmms_asf_read_GUID (guchar *buffer, xmms_asf_GUID_t *guid)
{
	guid->v1 = GUINT32_FROM_LE (*((guint32 *) buffer));
	buffer += 4;
	guid->v2 = GUINT16_FROM_LE (*((guint16 *) buffer));
	buffer += 2;
	guid->v3 = GUINT16_FROM_LE (*((guint16 *) buffer));
	buffer += 2;
	memcpy (guid->v4, buffer, sizeof (guid->v4));
}

static gboolean
xmms_asf_check_GUID (const xmms_asf_GUID_t *guid1,
                     const xmms_asf_GUID_t *guid2)
{
	return !memcmp (guid1, guid2, sizeof (xmms_asf_GUID_t));
}

/* This is a hack for reading ASF metadata since libavformat
 * totally breaks UTF-16 characters in them. */
static void
xmms_asf_metahack (xmms_xform_t *xform)
{
	gint i;
	guint objects;
	gulong read, bufsize;
	guchar *buffer, *current;
	xmms_error_t error;
	xmms_asf_GUID_t guid;

	g_return_if_fail (xform);

	bufsize = sizeof (xmms_asf_GUID_t) + 8+4+2;
	buffer = g_malloc (bufsize * sizeof (guchar));
	current = buffer;

	read = xmms_xform_peek (xform, buffer, bufsize, &error);
	g_return_if_fail (read == bufsize);

	xmms_asf_read_GUID (buffer, &guid);
	g_return_if_fail (xmms_asf_check_GUID (&guid, &xmms_asf_GUIDs[0]));
	current += sizeof (xmms_asf_GUID_t);;

	current += 8;
	objects = GUINT32_FROM_LE (*((guint32 *) current));

	for (i = 0; i < objects; i++) {
		gulong objectsize;
		gint headersize = sizeof (xmms_asf_GUID_t) + 8;

		bufsize += headersize;
		buffer = g_realloc (buffer, bufsize);
		current = buffer + bufsize - headersize;

		read = xmms_xform_peek (xform, buffer, bufsize, &error);
		g_return_if_fail (read == bufsize);

		xmms_asf_read_GUID (current, &guid);
		current += sizeof (xmms_asf_GUID_t);
		objectsize = GULONG_FROM_LE (*((gulong *) current));

		bufsize += objectsize - headersize;
		buffer = g_realloc (buffer, bufsize);
		current = buffer + bufsize - objectsize + headersize;

		read = xmms_xform_peek (xform, buffer, bufsize, &error);
		g_return_if_fail (read == bufsize);

		if (xmms_asf_check_GUID (&guid, &xmms_asf_GUIDs[1])) {
			guint16 titlel, artistl, copyl, commentl, ratingl;
			guint16 *tmpbuf = (guint16 *) current;
			gchar *utfstr, *tmpstr;

			titlel = GUINT16_FROM_LE (tmpbuf[0]);
			artistl = GUINT16_FROM_LE (tmpbuf[1]);
			copyl = GUINT16_FROM_LE (tmpbuf[2]);
			commentl = GUINT16_FROM_LE (tmpbuf[3]);
			ratingl = GUINT16_FROM_LE (tmpbuf[4]);

			tmpbuf+=5;

			utfstr = (gchar *) tmpbuf;
			if (titlel > 0) {
				tmpstr = g_convert (utfstr, titlel, "UTF-8", "UTF-16LE", NULL,
				                    NULL, NULL);
				if (tmpstr && strlen (tmpstr)) {
					xmms_xform_metadata_set_str (xform,
					                             XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,
					                             tmpstr);
				}
				g_free (tmpstr);
				utfstr += titlel;
			}
			if (artistl > 0) {
				tmpstr = g_convert (utfstr, artistl, "UTF-8", "UTF-16LE", NULL,
				                    NULL, NULL);
				if (tmpstr && strlen (tmpstr)) {
					xmms_xform_metadata_set_str (xform,
					                             XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,
					                             tmpstr);
				}
				g_free (tmpstr);
				utfstr += artistl;
			}
			utfstr += copyl;
			if (commentl > 0) {
				tmpstr = g_convert (utfstr, commentl, "UTF-8", "UTF-16LE",
				                    NULL, NULL, NULL);
				if (tmpstr && strlen (tmpstr)) {
					xmms_xform_metadata_set_str (xform,
					                             XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT,
					                             tmpstr);
				}
				g_free (tmpstr);
				utfstr += commentl;
			}
		} else if (xmms_asf_check_GUID (&guid, &xmms_asf_GUIDs[2])) {
			guint16 i, descriptors;

			descriptors = GUINT16_FROM_LE (*((guint16 *) current));
			current += 2;
			for (i=0; i < descriptors; i++) {
				guint16 namelen, datatype, datalen;
				gchar *name, *value = NULL;

				namelen = GUINT16_FROM_LE (*((guint16 *) current));
				current += 2;
				name = g_convert ((gchar *) current, namelen,
				                  "UTF-8", "UTF-16LE", NULL,
				                  NULL, NULL);
				current += namelen;
				datatype = GUINT16_FROM_LE (*((guint16 *) current));
				current += 2;
				datalen = GUINT16_FROM_LE (*((guint16 *) current));
				current += 2;
				if (datatype == 0x0000) {
					value = g_convert ((gchar *) current, datalen, "UTF-8",
					                   "UTF-16LE", NULL, NULL, NULL);
				}
				current += datalen;

				if (name == NULL || value == NULL || !strlen (value)) {
				} else if (!strcmp (name, "WM/AlbumTitle")) {
					xmms_xform_metadata_set_str (xform,
					                             XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,
					                             value);
				} else if (!strcmp (name, "WM/Year")) {
					xmms_xform_metadata_set_str (xform,
					                             XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,
					                             value);
				} else if (!strcmp (name, "WM/Genre")) {
					xmms_xform_metadata_set_str (xform,
					                             XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,
					                             value);
				} else if (!strcmp (name, "WM/Track")) {
					gint tracknr;
					gchar *end;

					tracknr = strtol (value, &end, 10);
					if (end && *end == '\0') {
						xmms_xform_metadata_set_int (xform,
						                             XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,
						                             tracknr);
					}
				}

				g_free (name);
				g_free (value);
			}
		}
	}

	g_free (buffer);
}


static void
xmms_avformat_get_mediainfo (xmms_xform_t *xform)
{
	xmms_avformat_data_t *data;
	AVFormatContext *fmtctx;
	AVCodecContext *codecctx;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	fmtctx = data->fmtctx;
	codecctx = data->codecctx;

	if (codecctx->bit_rate > 0) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
		                             codecctx->bit_rate);
	}
	if (fmtctx->streams[data->track]->duration > 0) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
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

