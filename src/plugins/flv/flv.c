/*  flv - A demuxer plugin for XMMS2
 *  Copyright (C) 2008 Anthony Garcia
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#define FLV_HDR_SIZE 9
#define HAS_AUDIO 0x04
/* each tag has an 11 byte header
 * followed by format specific metadata
 * a/v tags have a one byte header containing codecs
 * and other info, then data
 */
#define FLV_TAG_SIZE 11
/* random constant */
#define FLV_CHUNK_SIZE 4096

/* let libavcodec take care of swapping sample bytes */
static const gchar *mime_pcm_s16le = "audio/x-ffmpeg-pcm_s16le";
static const gchar *fmt_mime[11] = {
	/* Supported when samples are 8 bit
	 * (otherwise there's no way of knowing endianness)
	 */
	"audio/pcm",
	"audio/x-ffmpeg-adpcm_swf",
	"audio/mpeg",
	/* if bps is 8 bit u8
	 * if bps is 16 bit sle16
	 */
	"audio/pcm",
	/* libavcodec can't handle nelly without dying yet */
	/*"audio/x-ffmpeg-nellymoser",
	"audio/x-ffmpeg-nellymoser",
	"audio/x-ffmpeg-nellymoser",*/
	"", "", "",
	"", "", "",
	"audio/aac"
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
	xmms_sample_format_t bps = XMMS_SAMPLE_FORMAT_U8;
	gint readret = 0;
	guint8 channels = 1, flags = 0, format = 0;
	guint8 header[FLV_TAG_SIZE + 5];
	const gchar *mime = NULL;
	guint32 dataoffset = 0, samplerate = 0;
	xmms_error_t err;
	xmms_flv_data_t *flvdata = NULL;

	readret = xmms_xform_read (xform, header, FLV_HDR_SIZE, &err);
	if (readret != FLV_HDR_SIZE) {
		xmms_log_error ("Header read error");
		return FALSE;
	}

	if ((header[4] & HAS_AUDIO) != HAS_AUDIO) {
		xmms_log_error ("FLV has no audio stream");
		return FALSE;
	}

	dataoffset = get_be32 (&header[5]) - FLV_HDR_SIZE;
	/* there might be something between the header and
	 * tag body eventually
	 */
	while (dataoffset) {
		readret = xmms_xform_read (xform, header,
		                           (dataoffset < FLV_HDR_SIZE)?
		                           dataoffset : FLV_HDR_SIZE, &err);
		if (readret == -1) {
			xmms_log_error ("Error reading header:tag body gap");
			return FALSE;
		}

		dataoffset -= readret;
	}

	if (next_audio_tag (xform) <= 0) {
		xmms_log_error ("Can't find first audio tag");
		return FALSE;
	}

	if (xmms_xform_peek (xform, header, FLV_TAG_SIZE + 5, &err) < FLV_TAG_SIZE + 5) {
		xmms_log_error ("Can't read first audio tag");
		return FALSE;
	}

	flags = header[FLV_TAG_SIZE + 4];
	XMMS_DBG ("Audio flags: %X", flags);

	switch (flags&12) {
		case 0: samplerate = 5512; break;
		case 4: samplerate = 11025; break;
		case 8: samplerate = 22050; break;
		case 12: samplerate = 44100; break;
		default: samplerate = 8000; break;
	}

	if (flags&2) {
		bps = XMMS_SAMPLE_FORMAT_S16;
	}

	if (flags&1) {
		channels = 2;
	}

	format = flags >> 4;
	mime = (format <= 10)? fmt_mime[format] : NULL;
	switch (format) {
		case 0:
			/* If the flv has an HE PCM audio stream, the
			 * samples must be unsigned and 8 bits long
			 */
			if (bps != XMMS_SAMPLE_FORMAT_U8) {
				xmms_log_error ("Only u8 HE PCM is supported");
				return FALSE;
			}
			break;
		case 3:
			if (bps == XMMS_SAMPLE_FORMAT_S16) {
				mime = mime_pcm_s16le;
			}
			break;
	}

	if (mime && *mime) {
		flvdata = g_new0 (xmms_flv_data_t, 1);
		flvdata->format = format;

		XMMS_DBG ("Rate: %d, bps: %d, channels: %d", samplerate,
		          bps, channels);

		xmms_xform_private_data_set (xform, flvdata);
		xmms_xform_outdata_type_add (xform,
		                             XMMS_STREAM_TYPE_MIMETYPE,
					     mime,
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
		return FALSE;
	}
}

static gint
xmms_flv_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	gint ret = 0, thismuch = FLV_TAG_SIZE + 5;
	guint8 header[FLV_TAG_SIZE + 6], gap = 1;
	xmms_flv_data_t *data = NULL;

	data = xmms_xform_private_data_get (xform);

	if (!data->last_datasize) {
		xmms_xform_auxdata_barrier (xform);
		ret = next_audio_tag (xform);
		if (ret > 0) {
			if (data->format == 10) {
				thismuch++;
				gap++;
			}
			if (xmms_xform_read (xform, header, thismuch, err) == thismuch) {
				data->last_datasize = get_be24 (&header[5]) - gap;
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
	guint8 header[FLV_TAG_SIZE + 4];
	guint8 dumb[FLV_CHUNK_SIZE];
	gint ret = 0;
	xmms_error_t err;
	guint32 datasize = 0;

	do {
		/* there's a last 4 bytes at the end of an FLV giving the final
		 * tag's size, this isn't an error
		 */
		ret = xmms_xform_peek (xform, header, FLV_TAG_SIZE + 4, &err);
		if ((ret < FLV_TAG_SIZE) && (ret > -1)) {
			ret = 0;
			break;
		} else if (ret == -1) {
			xmms_log_error ("%s", xmms_error_message_get (&err));
			break;
		}

		if (header[4] == 8) {
			/* woo audio tag! */
			break;
		}

		ret = xmms_xform_read (xform, header, FLV_TAG_SIZE + 4, &err);
		if (ret <= 0) { return ret; }

		datasize = get_be24 (&header[5]);

		while (datasize) {
			ret = xmms_xform_read (xform, dumb,
			                         (datasize < FLV_CHUNK_SIZE) ?
			                         datasize : FLV_CHUNK_SIZE,
			                         &err);
			if (ret == 0) {
				xmms_log_error ("Data field short!");
				break;
			} else if (ret == -1) {
				xmms_log_error ("%s",
				                xmms_error_message_get (&err));
				break;
			}

			datasize -= ret;
		}

	} while (ret);

	return ret;
}
