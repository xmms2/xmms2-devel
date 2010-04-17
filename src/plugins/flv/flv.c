/*  flv - A demuxer plugin for XMMS2
 *  Copyright (C) 2008-2013 XMMS2 Team and Anthony Garcia
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

/* An FLV demuxer plugin */

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>

/* each tag has an 11 byte header
 * followed by format specific metadata
 * a/v tags have a one byte header containing codecs
 * and other info, then data
 */
#define FLV_TAG_SIZE 11
#define FLV_CHUNK_SIZE 4096
#define FLV_HEADER_SIZE 9
#define FLV_HAS_AUDIO 0x04

typedef enum {
	/* Only u8 bit samples since
	   there's no way to determine endianness
	*/
	CODEC_PCM_HOST,
	CODEC_ADPCM,
	CODEC_MP3,
	/* 8 bps: unsigned
	   16 bps: signed
	*/
	CODEC_PCM_LE,
	CODEC_NELLYMOSER_16K,
	CODEC_NELLYMOSER_8K,
	/* Uses the sample rate in
	   the tag as normal
	*/
	CODEC_NELLYMOSER,
	CODEC_AAC = 10
} xmms_flv_codec_id;

struct xmms_flv_codec_table {
	xmms_flv_codec_id id;
	const gchar *mime;
} static flv_codecs[] = {
	{CODEC_PCM_HOST, "audio/pcm"},
	{CODEC_ADPCM, "audio/x-ffmpeg-adpcm_swf"},
	{CODEC_MP3, "audio/mpeg"},
	/* Will be audio/x-ffmpeg-pcm_s16le if bps is 16 */
	{CODEC_PCM_LE, "audio/pcm"},
	{CODEC_NELLYMOSER_16K, "audio/x-ffmpeg-nellymoser"},
	{CODEC_NELLYMOSER_8K, "audio/x-ffmpeg-nellymoser"},
	{CODEC_NELLYMOSER, "audio/x-ffmpeg-nellymoser"},
	{CODEC_AAC, "audio/aac"}
};

typedef struct {
	/* last known data field size, gets decremented every read
	 * until 0(new tag) */
	guint32 last_datasize;
	/* format of the audio stream */
	guint8 format;
} xmms_flv_data_t;

static gboolean xmms_flv_setup (xmms_xform_plugin_t *xform);
static gboolean xmms_flv_init (xmms_xform_t *xform);
static gint xmms_flv_read (xmms_xform_t *xform, xmms_sample_t *buf,
                           gint len, xmms_error_t *err);
static void xmms_flv_destroy (xmms_xform_t *xform);
/* reads the stream and discards any tag and data that isn't audio,
 * stopping at the beginning of an audio tag.
 * won't have to rely on previous xform in the chain implementing seek
 * returns: a positive integer when an audio tag was reached, 0 on eos, -1 on error
 */
static gint next_audio_tag (xmms_xform_t *xform);

static guint32
get_be32 (guint8 *b)
{
	return (b[3]) | (b[2] << 8) | (b[1] << 16) | (b[0] << 24);
}

static guint32
get_be24 (guint8 *b)
{
	return (b[2]) | (b[1] << 8) | (b[0] << 16);
}

XMMS_XFORM_PLUGIN ("flv", "FLV demuxer", XMMS_VERSION,
                   "Extracts an audio stream from an FLV",
                   xmms_flv_setup)

static gboolean
xmms_flv_setup (xmms_xform_plugin_t *xform)
{
	xmms_xform_methods_t methods;
	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_flv_init;
	methods.read = xmms_flv_read;
	methods.destroy = xmms_flv_destroy;

	xmms_xform_plugin_methods_set (xform, &methods);

	xmms_xform_plugin_indata_add (xform, XMMS_STREAM_TYPE_MIMETYPE,
	                              "video/x-flv", NULL);

	xmms_magic_add ("FLV header", "video/x-flv", "0 string FLV", NULL);
	xmms_magic_extension_add ("video/x-flv", "*.flv");

	return TRUE;
}

static gboolean
xmms_flv_init (xmms_xform_t *xform)
{
	xmms_sample_format_t bps;
	gint readret, i;
	guint8 channels, flags, format;
	guint8 header[FLV_TAG_SIZE + 1];
	guint32 dataoffset, samplerate;
	xmms_error_t err;
	xmms_flv_data_t *flvdata;
	struct xmms_flv_codec_table *codec = NULL;

	flvdata = g_new0 (xmms_flv_data_t, 1);
	xmms_xform_private_data_set (xform, flvdata);

	readret = xmms_xform_read (xform, header, FLV_HEADER_SIZE, &err);
	if (readret != FLV_HEADER_SIZE) {
		xmms_log_error ("Header read error");
		goto init_err;
	}

	if ((header[4] & FLV_HAS_AUDIO) != FLV_HAS_AUDIO) {
		xmms_log_error ("FLV has no audio stream");
		goto init_err;
	}

	dataoffset = get_be32 (&header[5]) - FLV_HEADER_SIZE;
	/* there might be something between the header and
	 * tag body eventually
	 */
	while (dataoffset) {
		readret = xmms_xform_read (xform, header,
		                           (dataoffset < FLV_HEADER_SIZE)?
		                           dataoffset : FLV_HEADER_SIZE, &err);
		if (readret <= 0) {
			xmms_log_error ("Error reading header:tag body gap");
			goto init_err;
		}

		dataoffset -= readret;
	}

	if (next_audio_tag (xform) <= 0) {
		xmms_log_error ("Can't find first audio tag");
		goto init_err;
	}

	if (xmms_xform_read (xform, header, FLV_TAG_SIZE + 1, &err) < FLV_TAG_SIZE + 1) {
		xmms_log_error ("Can't read first audio tag");
		goto init_err;
	}

	flags = header[11];
	XMMS_DBG ("Audio flags: %X", flags);

	format = flags >> 4;
	for (i = 0; i < G_N_ELEMENTS (flv_codecs); i++) {
		if (flv_codecs[i].id == format) {
			codec = &flv_codecs[i];
			break;
		}
	}

	if (flags & 1) {
		channels = 2;
	} else {
		channels = 1;
	}

	if (flags & 2) {
		bps = XMMS_SAMPLE_FORMAT_S16;
	} else {
		bps = XMMS_SAMPLE_FORMAT_U8;
	}

	switch ((flags & 12) >> 2) {
		case 0: samplerate = 5512; break;
		case 1: samplerate = 11025; break;
		case 2: samplerate = 22050; break;
		case 3: samplerate = 44100; break;
		default: samplerate = 8000; break;
	}

	if (codec) {
		switch (codec->id) {
			case CODEC_PCM_HOST:
				if (bps != XMMS_SAMPLE_FORMAT_U8) {
					xmms_log_error ("Only u8 HE PCM is supported");
					goto init_err;
				}
				break;
			case CODEC_PCM_LE:
				if (bps == XMMS_SAMPLE_FORMAT_S16) {
					codec->mime = "audio/x-ffmpeg-pcm_s16le";
				}
				break;
			case CODEC_NELLYMOSER_16K:
				samplerate = 16000;
				break;
			case CODEC_NELLYMOSER_8K:
				samplerate = 8000;
				break;
			default:
				break;
		}

		flvdata->format = format;
		flvdata->last_datasize = get_be24 (&header[1]) - 1;

		XMMS_DBG ("Rate: %d, bps: %d, channels: %d", samplerate,
		          bps, channels);

		xmms_xform_outdata_type_add (xform,
		                             XMMS_STREAM_TYPE_MIMETYPE,
		                             codec->mime,
		                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
		                             samplerate,
		                             XMMS_STREAM_TYPE_FMT_FORMAT,
		                             bps,
		                             XMMS_STREAM_TYPE_FMT_CHANNELS,
		                             channels,
		                             XMMS_STREAM_TYPE_END);
		return TRUE;
	} else {
		xmms_log_error ("Unsupported audio format");
	}

init_err:
	g_free (flvdata);
	return FALSE;
}

static gint
xmms_flv_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	gint ret = 0, thismuch = FLV_TAG_SIZE + 1;
	guint8 header[FLV_TAG_SIZE + 1];
	xmms_flv_data_t *data = NULL;

	data = xmms_xform_private_data_get (xform);

	if (!data->last_datasize) {
		xmms_xform_auxdata_barrier (xform);
		ret = next_audio_tag (xform);
		if (ret > 0) {
			if (xmms_xform_read (xform, header, thismuch, err) == thismuch) {
				data->last_datasize = get_be24 (&header[1]) - 1;
			} else {
				xmms_log_error ("Need %d bytes", thismuch);
				return -1;
			}
		} else {
			return ret;
		}
	}

	if (len <= data->last_datasize) {
		thismuch = len;
	} else {
		thismuch = data->last_datasize;
	}
	/* XMMS_DBG ("Wants %d, getting %d", len, thismuch); */

	ret = xmms_xform_read (xform, buf, thismuch, err);
	data->last_datasize -= ret;
	if (ret == -1) {
		xmms_log_error ("Requested: %d, %s", thismuch,
		                xmms_error_message_get (err));
	}

	return ret;

}

static void
xmms_flv_destroy (xmms_xform_t *xform)
{
	xmms_flv_data_t *data;
	data = xmms_xform_private_data_get (xform);
	g_free (data);
}

static gint
next_audio_tag (xmms_xform_t *xform)
{
	guint8 header[FLV_TAG_SIZE];
	guint8 dumb[FLV_CHUNK_SIZE];
	gint ret = 0;
	xmms_error_t err;
	xmms_flv_data_t *data;

	data = xmms_xform_private_data_get (xform);

	do {
		/* If > 0 assume we're in the middle of a tag's data */
		if (!data->last_datasize) {
			/* There are 4 bytes before an actual tag giving
			   the previous tag's size. The first size in an
			   flv is always 0.
			*/
			if (xmms_xform_read (xform, header, 4, &err) != 4) {
				xmms_log_error ("Couldn't read last tag size");
				return -1;
			}

			ret = xmms_xform_peek (xform, header, FLV_TAG_SIZE, &err);
			if ((ret < FLV_TAG_SIZE) && (ret > -1)) {
				return 0;
			} else if (ret == -1) {
				xmms_log_error ("%s", xmms_error_message_get (&err));
				return ret;
			}

			if (header[0] == 8) {
				/* woo audio tag! */
				break;
			}

			if ((ret = xmms_xform_read (xform, header, FLV_TAG_SIZE, &err)) <= 0) {
				return ret;
			}

			data->last_datasize = get_be24 (&header[1]);
		}

		while (data->last_datasize) {
			ret = xmms_xform_read (xform, dumb,
			                       (data->last_datasize < FLV_CHUNK_SIZE) ?
			                       data->last_datasize : FLV_CHUNK_SIZE,
			                       &err);
			if (ret == 0) {
				xmms_log_error ("Data field short!");
				break;
			} else if (ret == -1) {
				xmms_log_error ("%s",
				                xmms_error_message_get (&err));
				break;
			}

			data->last_datasize -= ret;
		}

	} while (ret);

	return ret;
}
