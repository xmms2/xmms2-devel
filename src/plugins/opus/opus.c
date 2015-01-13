/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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
  * @file opusfile decoder.
  * @url http://git.xiph.org/?p=opusfile.git
  */

#include <glib.h>

#include <opusfile.h>

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>
#include <xmms/xmms_bindata.h>

#include <glib.h>

#include <math.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

/* basic_mappings, and mappings, coverart parsing */
#include "../vorbis_common/metadata.c"

typedef struct xmms_opus_data_St {
	OggOpusFile *opusfile;
	OpusFileCallbacks callbacks;
	const OpusHead *opushead;
	const OpusTags *opustags;
	gint current;
	int channels;
} xmms_opus_data_t;

/*
 * Function prototypes
 */

static void xmms_opus_set_duration (xmms_xform_t *xform, guint dur);
static gulong xmms_opus_op_read (OggOpusFile *of, gchar *buf, gint len,
                                   gint bigendian, gint sampsize, gint signd,
                                   gint *outbuf);
static gboolean xmms_opus_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_opus_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static gboolean xmms_opus_init (xmms_xform_t *decoder);
static void xmms_opus_destroy (xmms_xform_t *decoder);
static gint64 xmms_opus_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);

/*
 * Plugin header
 */


XMMS_XFORM_PLUGIN_DEFINE ("opus",
                          "Opus Decoder", XMMS_VERSION,
                          "Xiph's Ogg/Opus decoder",
                          xmms_opus_plugin_setup);


static gboolean
xmms_opus_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_opus_init;
	methods.destroy = xmms_opus_destroy;
	methods.read = xmms_opus_read;
	methods.seek = xmms_opus_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_metadata_mapper_init (xform_plugin,
	                                        basic_mappings,
	                                        G_N_ELEMENTS (basic_mappings),
	                                        mappings,
	                                        G_N_ELEMENTS (mappings));

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/ogg; codecs=opus",
	                              NULL);

	xmms_magic_add ("Opus header", "audio/ogg; codecs=opus",
	                "0 string OggS",
	                ">28 string OpusHead", NULL);

	xmms_magic_extension_add ("audio/ogg; codecs=opus", "*.opus");

	return TRUE;
}

static void
xmms_opus_destroy (xmms_xform_t *xform)
{
	xmms_opus_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	op_free (data->opusfile);
	g_free (data);
}

static int
opus_callback_read (void *datasource, unsigned char *ptr, int size)
{
	xmms_opus_data_t *data;
	xmms_xform_t *xform = datasource;
	xmms_error_t error;
	size_t ret;

	g_return_val_if_fail (xform, 0);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	ret = xmms_xform_read (xform, ptr, size, &error);

	return ret;
}

static int
opus_callback_seek (void *datasource, opus_int64 offset, int whence)
{
	xmms_xform_t *xform = datasource;
	xmms_error_t err;
	gint ret;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	if (whence == SEEK_CUR) {
		whence = XMMS_XFORM_SEEK_CUR;
	} else if (whence == SEEK_SET) {
		whence = XMMS_XFORM_SEEK_SET;
	} else if (whence == SEEK_END) {
		whence = XMMS_XFORM_SEEK_END;
	}

	ret = xmms_xform_seek (xform, (gint64) offset, whence, &err);

	return (ret == -1) ? -1 : 0;
}

static int
opus_callback_close (void *datasource)
{
	return 0;
}

static opus_int64
opus_callback_tell (void *datasource)
{
	xmms_xform_t *xform = datasource;
	xmms_error_t err;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	return xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &err);
}

static void
xmms_opus_read_metadata (xmms_xform_t *xform, xmms_opus_data_t *data)
{
	data->opushead = op_head (data->opusfile, -1);
	data->opustags = op_tags (data->opusfile, -1);
	data->channels = op_channel_count (data->opusfile, -1);

	gint i;

	if (!data->opustags)
		return;

	for (i = 0; i < data->opustags->comments; i++) {
		const gchar *ptr, *entry;
		gsize length;
		gchar key[64];

		entry = data->opustags->user_comments[i];
		length = data->opustags->comment_lengths[i];

		if (entry == NULL || *entry == '\0')
			continue;

		/* check whether it's a valid comment */
		ptr = memchr (entry, '=', length);
		if (ptr == NULL)
			continue;

		ptr++;

		g_strlcpy (key, entry, MIN (ptr - entry, sizeof (key)));

		if (!xmms_xform_metadata_mapper_match (xform, key, ptr, length - (ptr - entry))) {
			XMMS_DBG ("Unhandled tag '%s'", entry);
		}

	}
}

static gboolean
xmms_opus_init (xmms_xform_t *xform)
{
	xmms_opus_data_t *data;
	gint ret;
	guint playtime;
	const gchar *metakey;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_opus_data_t, 1),

	data->callbacks.read = opus_callback_read;
	data->callbacks.close = opus_callback_close;
	data->callbacks.tell = opus_callback_tell;
	data->callbacks.seek = opus_callback_seek;

	data->current = -1;

	xmms_xform_private_data_set (xform, data);

	data->opusfile = op_open_callbacks (xform, &data->callbacks, NULL, 0,
	                         &ret);
	if (ret) {
		return FALSE;
	}

	playtime = op_pcm_total (data->opusfile, -1) / 48000;

	if (playtime != OP_EINVAL) {
		gint filesize;

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
		if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
			xmms_opus_set_duration (xform, playtime);
		}
	}

	xmms_opus_read_metadata (xform, data);

	/*
	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_FLOAT,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             48000,
	                             XMMS_STREAM_TYPE_END);
	*/

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             48000,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gint
xmms_opus_read (xmms_xform_t *xform, gpointer buf, gint len,
                  xmms_error_t *err)
{
	gint ret = 0;
	gint c;
	xmms_opus_data_t *data;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = xmms_opus_op_read (data->opusfile, (gchar *) buf, len,
	                           G_BYTE_ORDER == G_BIG_ENDIAN,
	                           xmms_sample_size_get (XMMS_SAMPLE_FORMAT_S16),
	                           1, &c);

	if (ret < 0) {
		return -1;
	}

	if (ret && c != data->current) {
		xmms_opus_read_metadata (xform, data);
		data->current = c;
	}

	return ret;
}

static gint64
xmms_opus_seek (xmms_xform_t *xform, gint64 samples,
                  xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_opus_data_t *data;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	if (samples > op_pcm_total (data->opusfile, -1)) {
		xmms_log_error ("Trying to seek past end of stream");
		return -1;
	}

	op_pcm_seek (data->opusfile, samples);

	return samples;
}

static void
xmms_opus_set_duration (xmms_xform_t *xform, guint dur)
{
	xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
	                             dur * 1000);
}

static gulong
xmms_opus_op_read (OggOpusFile *of, gchar *buf, gint len, gint bigendian,
                     gint sampsize, gint signd, gint *outbuf)
{
	gulong ret;
	int channels;

	channels = op_channel_count (of, -1);

	do {								 /* FIXME buffer len / sample size */
		//ret = op_read_float (of, (float *)buf, len / sizeof(float), outbuf);
		ret = op_read (of, (opus_int16 *)buf, len / 2, outbuf);
	} while (ret == OP_HOLE);

	/* FIXME bytes from samples_read * channels * sample size */
	//ret = ret * channels * sizeof(float);
	ret = ret * channels * 2;

	return ret;
}
