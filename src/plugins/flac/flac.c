/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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



#include "xmms/xmms_defs.h"
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"

#include <string.h>
#include <math.h>
#include <FLAC/all.h>

#include <glib.h>

typedef struct xmms_flac_data_St {
	FLAC__SeekableStreamDecoder *flacdecoder;
	FLAC__StreamMetadata *vorbiscomment;
	guint channels;
	guint sample_rate;
	guint bit_rate;
	guint bits_per_sample;
	guint64 total_samples;
	gboolean is_seeking;

	GString *buffer;
} xmms_flac_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_flac_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_flac_read (xmms_xform_t *xform,
                            xmms_sample_t *buf,
                            gint len,
                            xmms_error_t *err);
static gboolean xmms_flac_init (xmms_xform_t *decoder);
static void xmms_flac_destroy (xmms_xform_t *decoder);
/*
static gboolean xmms_flac_seek (xmms_xform_t *decoder, guint samples);
*/

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN("flac",
                  "FLAC Decoder", XMMS_VERSION,
                  "Free Lossless Audio Codec decoder",
                  xmms_flac_plugin_setup);

static gboolean
xmms_flac_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_flac_init;
	methods.destroy = xmms_flac_destroy;
	methods.read = xmms_flac_read;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-flac",
	                              NULL);

	xmms_magic_add ("flac header", "audio/x-flac",
	                "0 string fLaC", NULL);

	return TRUE;
}

static FLAC__SeekableStreamDecoderReadStatus
flac_callback_read (const FLAC__SeekableStreamDecoder *flacdecoder, 
                    FLAC__byte buffer[], 
                    guint *bytes, 
                    void *client_data)
{
	xmms_xform_t *xform = (xmms_xform_t *) client_data;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (xform, FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR);

	ret = xmms_xform_read (xform, (gchar *)buffer, *bytes, &error);
	*bytes = ret;

	if (ret <= 0) {
		return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
	}

	return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

static FLAC__StreamDecoderWriteStatus
flac_callback_write (const FLAC__SeekableStreamDecoder *flacdecoder, 
                     const FLAC__Frame *frame, 
                     const FLAC__int32 * const buffer[], 
                     void *client_data)
{
	xmms_xform_t *xform = (xmms_xform_t *)client_data;
	xmms_flac_data_t *data;
	guint length = frame->header.blocksize * frame->header.channels * frame->header.bits_per_sample / 8;
	guint sample, channel, pos = 0;
	guint8 packed[length];
	guint16 *packed16 = (guint16 *) packed;

	data = xmms_xform_private_data_get (xform);

	if (data->is_seeking)
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

	for (sample = 0; sample < frame->header.blocksize; sample++) {
		for (channel = 0; channel < frame->header.channels; channel++) {
			switch (data->bits_per_sample) {
				case 8:
					packed[pos] = (guint8)buffer[channel][sample];
					break;
				case 16:
					packed16[pos] = (guint16)buffer[channel][sample];
					break;
			}
			pos++;
		}
	}

	g_string_append_len (data->buffer, (gchar *) packed, length);

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static FLAC__SeekableStreamDecoderTellStatus
flac_callback_tell (const FLAC__SeekableStreamDecoder *flacdecoder, 
                    FLAC__uint64 *offset, void *client_data)
{
	xmms_error_t err;
	xmms_xform_t *xform = (xmms_xform_t *) client_data;

	g_return_val_if_fail (xform, FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR);

	xmms_error_reset (&err);

	*offset = xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &err);

	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__SeekableStreamDecoderSeekStatus
flac_callback_seek (const FLAC__SeekableStreamDecoder *flacdecoder, 
                    FLAC__uint64 offset, void *client_data)
{
	xmms_error_t err;
	xmms_xform_t *xform = (xmms_xform_t *) client_data;
	gint retval;

	xmms_error_reset (&err);

	retval = xmms_xform_seek (xform, (gint64) offset, 
	                          XMMS_XFORM_SEEK_SET, &err);

	if (retval == -1)
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;

	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__SeekableStreamDecoderLengthStatus
flac_callback_length (const FLAC__SeekableStreamDecoder *flacdecoder, 
                      FLAC__uint64 *stream_length, void *client_data)
{
	xmms_xform_t *xform = (xmms_xform_t *) client_data;
	gint retval;

	retval = xmms_xform_metadata_get_int (xform,
										  XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE);
	*stream_length = retval;

	if (retval == -1)
		return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;

	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

static void
flac_callback_metadata (const FLAC__SeekableStreamDecoder *flacdecoder, 
                        const FLAC__StreamMetadata *metadata, 
                        void *client_data)
{
	xmms_flac_data_t *data;
	xmms_xform_t *xform = (xmms_xform_t *) client_data;
	guint64 filesize = xmms_xform_metadata_get_int (xform,
	                                                XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE);

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);

	switch (metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:	/* FLAC__metadata_object_clone ()? */
			data->bits_per_sample = metadata->data.stream_info.bits_per_sample;
			data->sample_rate = metadata->data.stream_info.sample_rate;
			data->channels = metadata->data.stream_info.channels;
			data->total_samples = metadata->data.stream_info.total_samples;
			if (filesize > 0 && data->total_samples) {
				data->bit_rate = (guint) (filesize * 8 *
								 (guint64) data->sample_rate /
								 (guint64) data->total_samples);
			}
			XMMS_DBG ("STREAMINFO: BPS %d. Samplerate: %d. Channels: %d.",
					data->bits_per_sample,
					data->sample_rate,
					data->channels);
			break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			data->vorbiscomment = FLAC__metadata_object_clone (metadata);
			break;
		default: break;
	}
}

static FLAC__bool
flac_callback_eof (const FLAC__SeekableStreamDecoder *flacdecoder, 
                   void *client_data)
{
	xmms_xform_t *xform = (xmms_xform_t *) client_data;

	g_return_val_if_fail (flacdecoder, TRUE);
	g_return_val_if_fail (xform, TRUE);

	return xmms_xform_iseos (xform);
}

static void
flac_callback_error (const FLAC__SeekableStreamDecoder *flacdecoder, 
                     FLAC__StreamDecoderErrorStatus status, 
                     void *client_data)
{
	xmms_xform_t *data = (xmms_xform_t *) client_data;

	g_return_if_fail (flacdecoder);
	g_return_if_fail (data);

	XMMS_DBG ("%s", FLAC__StreamDecoderErrorStatusString[status]);
}

typedef enum { STRING, INTEGER } ptype;
typedef struct {
	gchar *vname;
	gchar *xname;
	ptype type;
} props;

#define MUSICBRAINZ_VA_ID "89ad4ac3-39f7-470e-963a-56509c546377"

/** These are the properties that we extract from the comments */
static props properties[] = {
	{ "title",                XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,     STRING  },
	{ "artist",               XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,    STRING  },
	{ "album",                XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,     STRING  },
	{ "tracknumber",          XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,   INTEGER },
	{ "date",                 XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,      STRING  },
	{ "genre",                XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,     STRING  },
	{ "musicbrainz_albumid",  XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID,  STRING  },
	{ "musicbrainz_artistid", XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID, STRING  },
	{ "musicbrainz_trackid",  XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID,  STRING  },
};

static gboolean
xmms_flac_init (xmms_xform_t *xform)
{
	xmms_flac_data_t *data;
	xmms_sample_format_t sample_fmt;
	FLAC__bool retval;
	FLAC__SeekableStreamDecoderState init_status;
	gint current, num_comments;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_flac_data_t, 1);

	data->flacdecoder = FLAC__seekable_stream_decoder_new ();
	FLAC__seekable_stream_decoder_set_eof_callback (data->flacdecoder, 
	                                                flac_callback_eof);
	FLAC__seekable_stream_decoder_set_read_callback (data->flacdecoder, 
	                                                 flac_callback_read);
	FLAC__seekable_stream_decoder_set_seek_callback (data->flacdecoder, 
	                                                 flac_callback_seek);
	FLAC__seekable_stream_decoder_set_tell_callback (data->flacdecoder, 
	                                                 flac_callback_tell);
	FLAC__seekable_stream_decoder_set_write_callback (data->flacdecoder, 
	                                                  flac_callback_write);
	FLAC__seekable_stream_decoder_set_error_callback (data->flacdecoder, 
	                                                  flac_callback_error);
	FLAC__seekable_stream_decoder_set_length_callback (data->flacdecoder, 
	                                                   flac_callback_length);
	FLAC__seekable_stream_decoder_set_metadata_callback (data->flacdecoder, 
	                                                     flac_callback_metadata);

	FLAC__seekable_stream_decoder_set_client_data (data->flacdecoder, xform);

	xmms_xform_private_data_set (xform, data);

	init_status = FLAC__seekable_stream_decoder_init (data->flacdecoder);

	if (init_status != FLAC__SEEKABLE_STREAM_DECODER_OK) {
		const gchar *errmsg = FLAC__seekable_stream_decoder_get_resolved_state_string (data->flacdecoder);
		XMMS_DBG ("FLAC init failed: %s", errmsg);
		goto err;
	}

	retval = FLAC__seekable_stream_decoder_process_until_end_of_metadata (data->flacdecoder);
	if (retval == false)
		goto err;

	if (data->vorbiscomment != NULL) {
		num_comments = data->vorbiscomment->data.vorbis_comment.num_comments;

		for (current = 0; current < num_comments; current++) {
			gchar **s, *val;
			guint length;
			gint i = 0;

			s = g_strsplit ((gchar *)data->vorbiscomment->data.vorbis_comment.comments[current].entry, "=", 2);
			length = data->vorbiscomment->data.vorbis_comment.comments[current].length - strlen (s[0]) - 1;
			val = g_strndup (s[1], length);
			for (i = 0; i < G_N_ELEMENTS (properties); i++) {
				if ((g_strcasecmp (s[0], "MUSICBRAINZ_ALBUMARTISTID") == 0) &&
				    (g_strcasecmp (val, MUSICBRAINZ_VA_ID) == 0)) {
					xmms_xform_metadata_set_int (xform,
					                             XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION,
					                             1);
				} else if (g_strcasecmp (properties[i].vname, s[0]) == 0) {
					if (properties[i].type == INTEGER) {
						gint tmp = strtol (val, NULL, 10);
						xmms_xform_metadata_set_int (xform, properties[i].xname, tmp);
					} else {
						xmms_xform_metadata_set_str (xform, properties[i].xname, val);
					}
				}
			}
			g_free (val);
			g_strfreev (s);
		}
	}

	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE, 
	                             (gint) data->bit_rate);

	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION, 
	                             (gint) data->total_samples / data->sample_rate * 1000);

	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE, 
	                             data->sample_rate);

	if (data->bits_per_sample != 8 && data->bits_per_sample != 16) {
		goto err;
	}

	if (data->bits_per_sample == 8)
		sample_fmt = XMMS_SAMPLE_FORMAT_S8;
	else
		sample_fmt = XMMS_SAMPLE_FORMAT_S16;

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             sample_fmt,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->sample_rate,
	                             XMMS_STREAM_TYPE_END);

	data->buffer = g_string_new (NULL);

	return TRUE;

err:

	FLAC__seekable_stream_decoder_finish (data->flacdecoder);
	FLAC__seekable_stream_decoder_delete (data->flacdecoder);
	g_free (data);
	return FALSE;

}

static gint 
xmms_flac_read (xmms_xform_t *xform,
                xmms_sample_t *buf,
                gint len,
                xmms_error_t *err)
{
	xmms_flac_data_t *data;
	gboolean ret;
	guint32 size;

	g_return_val_if_fail (xform, FALSE);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	size = MIN (data->buffer->len, len);

	if (size <= 0) {
		ret = FLAC__seekable_stream_decoder_process_single (data->flacdecoder);
	}

	if (FLAC__seekable_stream_decoder_get_state (data->flacdecoder) == FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM)
		return 0;

	size = MIN (data->buffer->len, len);

	memcpy (buf, data->buffer->str, size);
	g_string_erase (data->buffer, 0, size);
	return size;
}

static gboolean
xmms_flac_seek (xmms_xform_t *xform, guint samples)
{
	xmms_flac_data_t *data;
	FLAC__bool res;

	g_return_val_if_fail (xform, FALSE);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	data->is_seeking = TRUE;
	res = FLAC__seekable_stream_decoder_seek_absolute (data->flacdecoder, (FLAC__uint64) samples);
	data->is_seeking = FALSE;

	return res;
}

static void
xmms_flac_destroy (xmms_xform_t *decoder)
{
	xmms_flac_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_xform_private_data_get (decoder);
	g_return_if_fail (data);

	if (data->vorbiscomment)
		FLAC__metadata_object_delete (data->vorbiscomment);

	g_string_free (data->buffer, TRUE);

	FLAC__seekable_stream_decoder_finish (data->flacdecoder);
	FLAC__seekable_stream_decoder_delete (data->flacdecoder);

	g_free (data);
}
