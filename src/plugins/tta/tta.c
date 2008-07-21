/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2005-2008 XMMS2 Team
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

/*
 * TTA file format specification can be downloaded from the website:
 * http://true-audio.com/TTA_Lossless_Audio_Codec_-_Format_Description
 */

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"

#define TTA_HEADER_SIZE 22

#define CRC32_POLYNOMIAL 0xedb88320

typedef struct {
	guint16 format;
	guint16 channels;
	guint16 samplebits;
	guint32 samplerate;
	guint32 totalsamples;

	guint32 nextframe;
	guint32 framelen;
	guint32 totalframes;
	guint32 *seektable;

	guint32 next_boundary;
} xmms_tta_data_t;

/*
 * Function prototypes
 */
static gboolean xmms_tta_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_tta_init (xmms_xform_t *decoder);
static void xmms_tta_destroy (xmms_xform_t *decoder);

static gint xmms_tta_read (xmms_xform_t *xform, xmms_sample_t *buffer,
                           gint len, xmms_error_t *err);
static gint64 xmms_tta_seek (xmms_xform_t *xform, gint64 samples,
                             xmms_xform_seek_mode_t whence,
                             xmms_error_t *err);

XMMS_XFORM_PLUGIN ("tta", "TTA parser", XMMS_VERSION,
                   "True Audio Codec TTA file format parser",
                   xmms_tta_plugin_setup);

static guint16
get_le16 (guchar *data)
{
	return (data[1] << 8) | data[0];
}

static guint32
get_le32 (guchar *data)
{
	return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

static guint32
get_crc32 (guchar *data, gint datalen)
{
	guint32 lookup[256], crc;
	gint i, j;

	/* calculate the lookup table, this could be done just once... */
	for (i=0; i<256; i++) {
		lookup[i] = i;

		/* xor polynomial for each bit that is set and shift right */
		for (j=0; j<8; j++) {
			lookup[i] = (lookup[i] >> 1) ^
		                    (lookup[i] & 1 ? CRC32_POLYNOMIAL : 0);
		}
	}

	/* calculate the actual crc value for given data */
	crc = 0xFFFFFFFF;
	for (i=0; i<datalen; i++) {
		crc = (crc >> 8) ^ lookup[(crc ^ data[i]) & 0xFF];
	}

	return (crc ^ 0xFFFFFFFF);
}

/*
 * Plugin header
 */
static gboolean
xmms_tta_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_tta_init;
	methods.destroy = xmms_tta_destroy;
	methods.read = xmms_tta_read;
	methods.seek = xmms_tta_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin, XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-tta", NULL);

	xmms_magic_add ("TTA header", "audio/x-tta", "0 string TTA1", NULL);

	return TRUE;
}

static gboolean
xmms_tta_init (xmms_xform_t *xform)
{
	xmms_tta_data_t *data;
	xmms_error_t error;
	gint32 header_crc32, seektable_crc32;
	guchar *buffer;
	gint buflen, ret, i;

	data = g_new0 (xmms_tta_data_t, 1);
	data->seektable = NULL;
	xmms_xform_private_data_set (xform, data);

	buffer = g_malloc (TTA_HEADER_SIZE);
	buflen = TTA_HEADER_SIZE;

	ret = xmms_xform_read (xform, buffer, buflen, &error);
	if (ret <= 0) {
	        xmms_log_error ("Reading TTA header failed");
		goto error;
	}

	data->format       = get_le16 (buffer + 4);
	data->channels     = get_le16 (buffer + 6);
	data->samplebits   = get_le16 (buffer + 8);
	data->samplerate   = get_le32 (buffer + 10);
	data->totalsamples = get_le32 (buffer + 14);

	header_crc32 = get_crc32 (buffer, TTA_HEADER_SIZE - 4);
	if (header_crc32 != get_le32 (buffer + 18)) {
		xmms_log_error ("CRC32 check for TTA file header failed!");
		goto error;
	}

	/* ehm, they really use 256/245 as a constant, check the docs */
	data->framelen = data->samplerate * 256 / 245;
	data->totalframes = data->totalsamples / data->framelen;
	if (data->totalsamples % data->framelen) {
	        /* last frame contains the leftover samples */
	        data->totalframes++;
	}

	/* reallocate the buffer to include seektable and seektable CRC32 */
	buflen = TTA_HEADER_SIZE + (data->totalframes + 1) * sizeof (guint32);
	buffer = g_realloc (buffer, buflen);

	ret = xmms_xform_read (xform,
	                       buffer + TTA_HEADER_SIZE,
	                       buflen - TTA_HEADER_SIZE,
	                       &error);
	if (ret <= 0) {
	        xmms_log_error ("Reading TTA seektable failed");
		goto error;
	}
	seektable_crc32 = get_crc32 (buffer + TTA_HEADER_SIZE,
	                             buflen - TTA_HEADER_SIZE - 4);
	if (seektable_crc32 != get_le32 (buffer + buflen - 4)) {
		xmms_log_error ("CRC32 check for seektable failed, please re-encode "
		                "this TTA file to fix the header problems");
		goto error;
	}

	/* copy the seektable directly to our seektable array, allocate one
	 * extra integer to hold the end position of last frame */
	data->seektable = g_malloc ((data->totalframes + 1) * sizeof (guint32));
	memcpy (data->seektable + 1,
	        buffer + TTA_HEADER_SIZE,
	        data->totalframes * sizeof (guint32));

	/* make seektable incremental and swap endianness correctly, first frame
	 * is located immediately after header and seektable, same as buflen */
	data->seektable[0] = buflen;
	for (i=1; i <= data->totalframes; i++) {
		data->seektable[i] = GUINT32_FROM_LE (data->seektable[i]);
		if (data->seektable[i] < 4) {
			xmms_log_error ("Frame size in seektable too small, broken file");
			goto error;
		}
		data->seektable[i] += data->seektable[i-1];
	}

	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
	                             data->totalsamples / data->samplerate * 1000);

	xmms_xform_auxdata_set_int (xform,
	                            "samplebits",
	                            data->samplebits);

	/* avcodec likes to take the whole header as its config */
	xmms_xform_auxdata_set_bin (xform, "decoder_config", buffer, buflen);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/x-ffmpeg-tta",
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->samplerate,
	                             XMMS_STREAM_TYPE_END);

	g_free (buffer);

	return TRUE;

error:
	g_free (buffer);

	return FALSE;
}

static gint
xmms_tta_read (xmms_xform_t *xform, xmms_sample_t *buffer,
               gint len, xmms_error_t *err)
{
	xmms_tta_data_t *data;
	guint size, ret;

	g_return_val_if_fail (xform, -1);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	if (data->next_boundary == 0) {
		if (data->nextframe >= data->totalframes) {
			XMMS_DBG ("EOF");
			return 0;
		}

		/* this is the beginning of a new frame */
		xmms_xform_auxdata_barrier (xform);

		data->next_boundary = data->seektable[data->nextframe + 1] -
		                      data->seektable[data->nextframe];
		data->nextframe++;
	}

	size = MIN (data->next_boundary, len);

	ret = xmms_xform_read (xform, buffer, size, err);
	if (ret <= 0) {
		xmms_log_error ("Unexpected error reading frame data");
		return ret;
	}

	data->next_boundary -= ret;

	return size;
}

static gint64
xmms_tta_seek (xmms_xform_t *xform, gint64 samples,
               xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_tta_data_t *data;
	gint64 ret;
	gint idx;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);
	g_return_val_if_fail (data->seektable, -1);

	if (samples < 0 || samples > data->totalsamples) {
		/* trying to seek outside file bounds */
		xmms_error_set (err, XMMS_ERROR_GENERIC,
		                "Seek index out of bounds, only seek within the file");
		return -1;
	}

	/* calculate the position of the next frame */
	idx = samples / data->framelen;
	ret = xmms_xform_seek (xform,
	                       data->seektable[idx],
	                       XMMS_XFORM_SEEK_SET,
	                       err);
	if (ret != data->seektable[idx]) {
		xmms_log_error ("Seeking to the beginning of next frame failed");
		return -1;
	}

	/* update the index of the next frame after successful seek */
	data->nextframe = idx;

	return (data->nextframe * data->framelen);
}

void
xmms_tta_destroy (xmms_xform_t *xform)
{
	xmms_tta_data_t *data;

	g_return_if_fail (xform);
	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	g_free (data->seektable);
	g_free (data);
}
