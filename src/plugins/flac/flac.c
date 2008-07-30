/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"
#include "xmms/xmms_bindata.h"

#include <string.h>
#include <math.h>
#include <FLAC/all.h>

#include <glib.h>

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
# define FLAC__STREAM_DECODER_SEEK_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK
# define FLAC__STREAM_DECODER_SEEK_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR
# define FLAC__STREAM_DECODER_TELL_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK
# define FLAC__STREAM_DECODER_TELL_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR
# define FLAC__STREAM_DECODER_LENGTH_STATUS_OK FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK
# define FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR
# define FLAC__STREAM_DECODER_END_OF_STREAM FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM
# define FLAC__StreamDecoder FLAC__SeekableStreamDecoder
# define FLAC__StreamDecoderState FLAC__SeekableStreamDecoderState
# define FLAC__StreamDecoderReadStatus FLAC__SeekableStreamDecoderReadStatus
# define FLAC__StreamDecoderTellStatus FLAC__SeekableStreamDecoderTellStatus
# define FLAC__StreamDecoderSeekStatus FLAC__SeekableStreamDecoderSeekStatus
# define FLAC__StreamDecoderLengthStatus FLAC__SeekableStreamDecoderLengthStatus
# define FLAC__stream_decoder_new FLAC__seekable_stream_decoder_new
# define FLAC__stream_decoder_set_metadata_respond_all FLAC__seekable_stream_decoder_set_metadata_respond_all
# define FLAC__stream_decoder_finish FLAC__seekable_stream_decoder_finish
# define FLAC__stream_decoder_delete FLAC__seekable_stream_decoder_delete
# define FLAC__stream_decoder_process_single FLAC__seekable_stream_decoder_process_single
# define FLAC__stream_decoder_get_state FLAC__seekable_stream_decoder_get_state
# define FLAC__stream_decoder_seek_absolute FLAC__seekable_stream_decoder_seek_absolute
# define FLAC__stream_decoder_process_until_end_of_metadata FLAC__seekable_stream_decoder_process_until_end_of_metadata
typedef unsigned read_callback_size_t;
#else
typedef size_t read_callback_size_t;
#endif

typedef struct xmms_flac_data_St {
	FLAC__StreamDecoder *flacdecoder;
	FLAC__StreamMetadata *vorbiscomment;
	guint channels;
	guint sample_rate;
	guint bit_rate;
	guint bits_per_sample;
	guint64 total_samples;

	GString *buffer;
} xmms_flac_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_flac_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_flac_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                            xmms_error_t *err);
static gboolean xmms_flac_init (xmms_xform_t *xform);
static void xmms_flac_destroy (xmms_xform_t *xform);
static gint64 xmms_flac_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("flac",
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
	methods.seek = xmms_flac_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-flac",
	                              NULL);

	xmms_magic_add ("flac header", "audio/x-flac",
	                "0 string fLaC", NULL);

	return TRUE;
}

static FLAC__StreamDecoderReadStatus
flac_callback_read (const FLAC__StreamDecoder *flacdecoder,
                    FLAC__byte buffer[],
                    read_callback_size_t *bytes,
                    void *client_data)
{
	xmms_xform_t *xform = (xmms_xform_t *) client_data;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (xform,
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	                      FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR);
#else
	                      FLAC__STREAM_DECODER_READ_STATUS_ABORT);
#endif

	ret = xmms_xform_read (xform, (gchar *)buffer, *bytes, &error);
	*bytes = ret;

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	return (ret <= 0) ? FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR
	                  : FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
#else
	return (ret <= 0) ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
	                  : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
#endif
}

static FLAC__StreamDecoderWriteStatus
flac_callback_write (const FLAC__StreamDecoder *flacdecoder,
                     const FLAC__Frame *frame,
                     const FLAC__int32 * const buffer[],
                     void *client_data)
{
	xmms_xform_t *xform = (xmms_xform_t *)client_data;
	xmms_flac_data_t *data;
	guint sample, channel;
	guint8 packed;
	guint16 packed16;

	data = xmms_xform_private_data_get (xform);

	for (sample = 0; sample < frame->header.blocksize; sample++) {
		for (channel = 0; channel < frame->header.channels; channel++) {
			switch (data->bits_per_sample) {
				case 8:
					packed = (guint8)buffer[channel][sample];
					g_string_append_len (data->buffer, (gchar *) &packed, 1);
					break;
				case 16:
					packed16 = (guint16)buffer[channel][sample];
					g_string_append_len (data->buffer, (gchar *) &packed16, 2);
					break;
			}
		}
	}


	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static FLAC__StreamDecoderTellStatus
flac_callback_tell (const FLAC__StreamDecoder *flacdecoder,
                    FLAC__uint64 *offset, void *client_data)
{
	xmms_error_t err;
	xmms_xform_t *xform = (xmms_xform_t *) client_data;

	g_return_val_if_fail (xform,
	                      FLAC__STREAM_DECODER_TELL_STATUS_ERROR);

	xmms_error_reset (&err);

	*offset = xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &err);

	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderSeekStatus
flac_callback_seek (const FLAC__StreamDecoder *flacdecoder,
                    FLAC__uint64 offset, void *client_data)
{
	xmms_error_t err;
	xmms_xform_t *xform = (xmms_xform_t *) client_data;
	gint retval;

	xmms_error_reset (&err);

	retval = xmms_xform_seek (xform, (gint64) offset,
	                          XMMS_XFORM_SEEK_SET, &err);

	return (retval == -1) ? FLAC__STREAM_DECODER_SEEK_STATUS_ERROR
	                      : FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus
flac_callback_length (const FLAC__StreamDecoder *flacdecoder,
                      FLAC__uint64 *stream_length, void *client_data)
{
	xmms_xform_t *xform = (xmms_xform_t *) client_data;
	const gchar *metakey;
	gint val;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (xmms_xform_metadata_get_int (xform, metakey, &val)) {
		*stream_length = val;
		return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
	}

	return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
}

static void
flac_callback_metadata (const FLAC__StreamDecoder *flacdecoder,
                        const FLAC__StreamMetadata *metadata,
                        void *client_data)
{
	xmms_flac_data_t *data;
	xmms_xform_t *xform = (xmms_xform_t *) client_data;
	gint32 filesize;
	const gchar *metakey;

	g_return_if_fail (xform);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (!xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		filesize = -1;
	}

	data = xmms_xform_private_data_get (xform);

	switch (metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			/* FLAC__metadata_object_clone ()? */
			data->bits_per_sample = metadata->data.stream_info.bits_per_sample;
			data->sample_rate = metadata->data.stream_info.sample_rate;
			data->channels = metadata->data.stream_info.channels;
			data->total_samples = metadata->data.stream_info.total_samples;

			if (filesize > 0 && data->total_samples) {
				data->bit_rate = (guint) ((guint64) filesize * 8 *
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
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT > 7
		case FLAC__METADATA_TYPE_PICTURE: {
			gchar hash[33];
			if (metadata->data.picture.type == FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER &&
			    xmms_bindata_plugin_add (metadata->data.picture.data,
			                             metadata->data.picture.data_length,
			                             hash)) {
				const gchar *metakey;
				
				metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT;
				xmms_xform_metadata_set_str (xform, metakey, hash);
				
				metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT_MIME;
				xmms_xform_metadata_set_str (xform, metakey, metadata->data.picture.mime_type);
			}
			break;
		}
#endif
		/* if we want to support more metadata types here,
		 * don't forget to add a call to
		 * FLAC__stream_decoder_set_metadata_respond() below.
		 */
		default:
			break;
	}
}

static FLAC__bool
flac_callback_eof (const FLAC__StreamDecoder *flacdecoder,
                   void *client_data)
{
	xmms_xform_t *xform = (xmms_xform_t *) client_data;

	g_return_val_if_fail (flacdecoder, TRUE);
	g_return_val_if_fail (xform, TRUE);

	return xmms_xform_iseos (xform);
}

static void
flac_callback_error (const FLAC__StreamDecoder *flacdecoder,
                     FLAC__StreamDecoderErrorStatus status,
                     void *client_data)
{
	xmms_xform_t *data = (xmms_xform_t *) client_data;

	g_return_if_fail (flacdecoder);
	g_return_if_fail (data);

	XMMS_DBG ("%s", FLAC__StreamDecoderErrorStatusString[status]);
}

typedef enum { STRING, INTEGER, RPGAIN } ptype;
typedef struct {
	const gchar *vname;
	const gchar *xname;
	ptype type;
} props;

#define MUSICBRAINZ_VA_ID "89ad4ac3-39f7-470e-963a-56509c546377"

/** These are the properties that we extract from the comments */
static const props properties[] = {
	{ "title",                XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,     STRING  },
	{ "artist",               XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,    STRING  },
	{ "album",                XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,     STRING  },
	{ "tracknumber",          XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,   INTEGER },
	{ "date",                 XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,      STRING  },
	{ "genre",                XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,     STRING  },
	{ "comment",              XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT,   STRING  },
	{ "description",          XMMS_MEDIALIB_ENTRY_PROPERTY_DESCRIPTION, STRING  },
	{ "discnumber",           XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET, INTEGER },
	{ "musicbrainz_albumid",  XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID,  STRING  },
	{ "musicbrainz_artistid", XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID, STRING  },
	{ "musicbrainz_trackid",  XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID,  STRING  },
	{ "replaygain_track_gain",XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK,RPGAIN  },
	{ "replaygain_album_gain",XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,RPGAIN  },
	{ "replaygain_track_peak",XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,STRING  },
	{ "replaygain_album_peak",XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM,STRING  },
};

/* note that "key" is NOT NUL-terminated here,
 * but "value" is.
 */
static void
handle_comment (xmms_xform_t *xform,
                const gchar *key, gint key_len, const gchar *value)
{
	gchar buf[8];
	gint i;

	for (i = 0; i < G_N_ELEMENTS (properties); i++) {
		if ((!g_ascii_strncasecmp (key, "MUSICBRAINZ_ALBUMARTISTID", key_len)) &&
		    (!g_ascii_strcasecmp (value, MUSICBRAINZ_VA_ID))) {
			const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION;
			xmms_xform_metadata_set_int (xform, metakey, 1);
		} else if (!g_ascii_strncasecmp (key, properties[i].vname, key_len)) {
			if (properties[i].type == INTEGER) {
				gint tmp = strtol (value, NULL, 10);
				xmms_xform_metadata_set_int (xform,
				                             properties[i].xname, tmp);
			} else if (properties[i].type == RPGAIN) {
				g_snprintf (buf, sizeof (buf), "%f",
				            pow (10.0, g_strtod (value, NULL) / 20));

				/** @todo this should probably be a int instead? */
				xmms_xform_metadata_set_str (xform,
				                             properties[i].xname, buf);
			} else {
				xmms_xform_metadata_set_str (xform,
				                             properties[i].xname, value);
			}
		}
	}
}

static void
handle_comments (xmms_xform_t *xform, xmms_flac_data_t *data)
{
	FLAC__StreamMetadata_VorbisComment *vc;
	gint i;

	g_return_if_fail (data->vorbiscomment);

	vc = &data->vorbiscomment->data.vorbis_comment;

	for (i = 0; i < vc->num_comments; i++) {
		FLAC__byte *ptr, *content = vc->comments[i].entry;
		gint j;

		/* check whether it's a valid comment */
		if (!content || !*content || *content == '=')
			continue;

		for (ptr = content, j = 0; j < vc->comments[i].length; ptr++, j++)
			if (*ptr == '=')
				break;

		if (j == vc->comments[i].length)
			continue;

		handle_comment (xform,
		                (gchar *) content, j,
		                (gchar *) ptr + 1);
	}
}

static gboolean
xmms_flac_init (xmms_xform_t *xform)
{
	xmms_flac_data_t *data;
	xmms_sample_format_t sample_fmt;
	FLAC__bool retval;
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__StreamDecoderState init_status;
#else
	FLAC__StreamDecoderInitStatus init_status;
#endif
	gint filesize;
	const gchar *metakey;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_flac_data_t, 1);

	xmms_xform_private_data_set (xform, data);

	data->flacdecoder = FLAC__stream_decoder_new ();

	/* we don't need to explicitly tell the decoder to respond to
	 * FLAC__METADATA_TYPE_STREAMINFO here, it always does.
	 */
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
	FLAC__seekable_stream_decoder_set_metadata_respond (data->flacdecoder,
	                                                    FLAC__METADATA_TYPE_VORBIS_COMMENT);
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

	init_status = FLAC__seekable_stream_decoder_init (data->flacdecoder);

	if (init_status != FLAC__SEEKABLE_STREAM_DECODER_OK) {
		const gchar *errmsg = FLAC__seekable_stream_decoder_get_resolved_state_string (data->flacdecoder);
		XMMS_DBG ("FLAC init failed: %s", errmsg);
		goto err;
	}
#else
	FLAC__stream_decoder_set_metadata_respond (data->flacdecoder,
	                                           FLAC__METADATA_TYPE_VORBIS_COMMENT);
	FLAC__stream_decoder_set_metadata_respond (data->flacdecoder,
	                                           FLAC__METADATA_TYPE_PICTURE);

	init_status =
		FLAC__stream_decoder_init_stream (data->flacdecoder,
		                                  flac_callback_read,
		                                  flac_callback_seek,
		                                  flac_callback_tell,
		                                  flac_callback_length,
		                                  flac_callback_eof,
		                                  flac_callback_write,
		                                  flac_callback_metadata,
		                                  flac_callback_error,
		                                  xform);

	if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		XMMS_DBG ("FLAC init failed: %s",
		          FLAC__stream_decoder_get_resolved_state_string (data->flacdecoder));
		goto err;
	}
#endif

	retval = FLAC__stream_decoder_process_until_end_of_metadata (data->flacdecoder);
	if (!retval)
		goto err;

	if (data->vorbiscomment) {
		handle_comments (xform, data);
	}

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
	xmms_xform_metadata_set_int (xform, metakey, (gint) data->bit_rate);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		gint32 val = (gint32) data->total_samples / data->sample_rate * 1000;

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		xmms_xform_metadata_set_int (xform, metakey, val);
	}

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

	FLAC__stream_decoder_finish (data->flacdecoder);
	FLAC__stream_decoder_delete (data->flacdecoder);
	g_free (data);
	xmms_xform_private_data_set (xform, NULL);

	return FALSE;

}

static gint
xmms_flac_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                xmms_error_t *err)
{
	FLAC__StreamDecoderState state;
	xmms_flac_data_t *data;
	gboolean ret;
	guint32 size;

	g_return_val_if_fail (xform, FALSE);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	size = MIN (data->buffer->len, len);

	if (size <= 0) {
		ret = FLAC__stream_decoder_process_single (data->flacdecoder);
	}

	state = FLAC__stream_decoder_get_state (data->flacdecoder);

	if (state == FLAC__STREAM_DECODER_END_OF_STREAM) {
		return 0;
	}

	size = MIN (data->buffer->len, len);

	memcpy (buf, data->buffer->str, size);
	g_string_erase (data->buffer, 0, size);

	return size;
}

static gint64
xmms_flac_seek (xmms_xform_t *xform, gint64 samples,
                xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_flac_data_t *data;
	FLAC__bool res;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	if (samples > data->total_samples) {
		xmms_log_error ("Trying to seek past end of stream");
		return -1;
	}

	res = FLAC__stream_decoder_seek_absolute (data->flacdecoder,
	                                          (FLAC__uint64) samples);

	return res ? samples : -1;
}

static void
xmms_flac_destroy (xmms_xform_t *xform)
{
	xmms_flac_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->vorbiscomment) {
		FLAC__metadata_object_delete (data->vorbiscomment);
	}

	g_string_free (data->buffer, TRUE);

	FLAC__stream_decoder_finish (data->flacdecoder);
	FLAC__stream_decoder_delete (data->flacdecoder);

	g_free (data);
}
