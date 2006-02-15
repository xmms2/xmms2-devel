/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003  Peter Alm, Tobias Rundström, Anders Gustafsson
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
#include "xmms/xmms_decoderplugin.h"
#include "xmms/xmms_log.h"

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
} xmms_flac_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_flac_new (xmms_decoder_t *decoder);
static gboolean xmms_flac_init (xmms_decoder_t *decoder, gint mode);
static gboolean xmms_flac_seek (xmms_decoder_t *decoder, guint samples);
static gboolean xmms_flac_decode_block (xmms_decoder_t *decoder);
static void xmms_flac_destroy (xmms_decoder_t *decoder);
static void xmms_flac_get_mediainfo (xmms_decoder_t *decoder);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, 
				  XMMS_DECODER_PLUGIN_API_VERSION,
				  "flac",
				  "FLAC decoder " XMMS_VERSION,
				  "Free Lossless Audio Codec decoder");
	
	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://flac.sourceforge.net/");
	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_flac_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_flac_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_flac_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_flac_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_flac_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_flac_get_mediainfo);

	/*
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);
	*/

	xmms_plugin_magic_add (plugin, "flac header", "audio/x-flac",
	                       "0 string fLaC", NULL);

	return plugin;
}

FLAC__SeekableStreamDecoderReadStatus
flac_callback_read (const FLAC__SeekableStreamDecoder *flacdecoder, FLAC__byte buffer[], guint *bytes, void *client_data)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) client_data;
	xmms_transport_t *transport;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (decoder, FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR);

	ret = xmms_transport_read (transport, (gchar *)buffer, *bytes, &error);
	*bytes = ret;

	if (ret <= 0) {
		return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR;
	}

	return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

FLAC__StreamDecoderWriteStatus
flac_callback_write (const FLAC__SeekableStreamDecoder *flacdecoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *)client_data;
	xmms_flac_data_t *data;
	guint length = frame->header.blocksize * frame->header.channels * frame->header.bits_per_sample / 8;
	guint sample, channel, pos = 0;
	guint8 packed[length];
	guint16 *packed16 = (guint16*)packed;

	data = xmms_decoder_private_data_get (decoder);

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

	xmms_decoder_write (decoder, (gchar *)packed, length);

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

FLAC__SeekableStreamDecoderTellStatus
flac_callback_tell (const FLAC__SeekableStreamDecoder *flacdecoder, FLAC__uint64 *offset, void *client_data)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) client_data;
	xmms_transport_t *transport;

	transport = xmms_decoder_transport_get (decoder);

	*offset = xmms_transport_tell (transport);

	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__SeekableStreamDecoderSeekStatus
flac_callback_seek (const FLAC__SeekableStreamDecoder *flacdecoder, FLAC__uint64 offset, void *client_data)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) client_data;
	xmms_transport_t *transport;
	gint retval;

	transport = xmms_decoder_transport_get (decoder);

	if (xmms_transport_can_seek (transport) == FALSE)
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;

	retval = xmms_transport_seek (transport, offset, XMMS_TRANSPORT_SEEK_SET);

	if (retval == -1)
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;

	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__SeekableStreamDecoderLengthStatus
flac_callback_length (const FLAC__SeekableStreamDecoder *flacdecoder, FLAC__uint64 *stream_length, void *client_data)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) client_data;
	xmms_transport_t *transport;
	gint retval;

	transport = xmms_decoder_transport_get (decoder);

	retval = xmms_transport_size (transport);
	*stream_length = retval;

	if (retval == -1)
		return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR;

	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

void
flac_callback_metadata (const FLAC__SeekableStreamDecoder *flacdecoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	xmms_decoder_t *decoder = (xmms_decoder_t *) client_data;
	xmms_transport_t *transport = xmms_decoder_transport_get (decoder);
	/* xmms_transport_size checks that transport exists internally */
	guint64 filesize = xmms_transport_size (transport);
	xmms_flac_data_t *data;

	g_return_if_fail (transport);

	data = xmms_decoder_private_data_get (decoder);

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

FLAC__bool
flac_callback_eof (const FLAC__SeekableStreamDecoder *flacdecoder, void *client_data)
{
	xmms_decoder_t *data = (xmms_decoder_t *) client_data;
	xmms_transport_t *transport;

	g_return_val_if_fail (flacdecoder, TRUE);
	g_return_val_if_fail (data, TRUE);

	transport = xmms_decoder_transport_get (data);
	g_return_val_if_fail (transport, TRUE);

	return xmms_transport_iseos (transport);
}

void
flac_callback_error (const FLAC__SeekableStreamDecoder *flacdecoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	xmms_decoder_t *data = (xmms_decoder_t *) client_data;

	g_return_if_fail (flacdecoder);
	g_return_if_fail (data);

	XMMS_DBG ("%s", FLAC__StreamDecoderErrorStatusString[status]);
}

static gboolean
xmms_flac_new (xmms_decoder_t *decoder)
{
	xmms_flac_data_t *data;

	data = g_new0 (xmms_flac_data_t, 1);

	data->flacdecoder = FLAC__seekable_stream_decoder_new ();
	FLAC__seekable_stream_decoder_set_eof_callback (data->flacdecoder, flac_callback_eof);
	FLAC__seekable_stream_decoder_set_read_callback (data->flacdecoder, flac_callback_read);
	FLAC__seekable_stream_decoder_set_seek_callback (data->flacdecoder, flac_callback_seek);
	FLAC__seekable_stream_decoder_set_tell_callback (data->flacdecoder, flac_callback_tell);
	FLAC__seekable_stream_decoder_set_write_callback (data->flacdecoder, flac_callback_write);
	FLAC__seekable_stream_decoder_set_error_callback (data->flacdecoder, flac_callback_error);
	FLAC__seekable_stream_decoder_set_length_callback (data->flacdecoder, flac_callback_length);
	FLAC__seekable_stream_decoder_set_metadata_callback (data->flacdecoder, flac_callback_metadata);

	FLAC__seekable_stream_decoder_set_client_data (data->flacdecoder, decoder);

	xmms_decoder_private_data_set (decoder, data);

	return TRUE;
}

static gboolean
xmms_flac_init (xmms_decoder_t *decoder, gint mode)
{
	xmms_flac_data_t *data;
	xmms_sample_format_t sample_fmt;
	FLAC__bool retval;
	FLAC__SeekableStreamDecoderState init_status;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	FLAC__seekable_stream_decoder_set_metadata_respond_all (data->flacdecoder);

	init_status = FLAC__seekable_stream_decoder_init (data->flacdecoder);

	if (init_status != FLAC__SEEKABLE_STREAM_DECODER_OK) {
		const gchar *errmsg = FLAC__seekable_stream_decoder_get_resolved_state_string (data->flacdecoder);
		XMMS_DBG ("FLAC init failed: %s", errmsg);
		return FALSE;
	}

	retval = FLAC__seekable_stream_decoder_process_until_end_of_metadata (data->flacdecoder);
	if (retval == false)
		return FALSE;

	if (data->bits_per_sample != 8 && data->bits_per_sample != 16)
		return FALSE;

	if (data->bits_per_sample == 8)
		sample_fmt = XMMS_SAMPLE_FORMAT_S8;
	else
		sample_fmt = XMMS_SAMPLE_FORMAT_S16;

	if (mode & XMMS_DECODER_INIT_DECODING) {
		xmms_decoder_format_add (decoder, sample_fmt, data->channels, data->sample_rate);
		if (xmms_decoder_format_finish (decoder) == NULL) {
			return FALSE;
		}
	}

	return TRUE;
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

static void
xmms_flac_get_mediainfo (xmms_decoder_t *decoder)
{
	xmms_flac_data_t *data;
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;
	gint current, num_comments;

	g_return_if_fail (decoder);

	session = xmms_medialib_begin_write ();

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	entry = xmms_decoder_medialib_entry_get (decoder);
	
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
					xmms_medialib_entry_property_set_int (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION, 1);
				} else if (g_strcasecmp (properties[i].vname, s[0]) == 0) {
					if (properties[i].type == INTEGER) {
						gint tmp = strtol (val, NULL, 10);
						xmms_medialib_entry_property_set_int (session, entry,
						                                      properties[i].xname, tmp);
					} else {
						xmms_medialib_entry_property_set_str (session, entry,
						                                      properties[i].xname, val);
					}
				}
			}
			g_free (val);
			g_strfreev (s);
		}
	}

	xmms_medialib_entry_property_set_int (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE, 
										  (gint) data->bit_rate);

	xmms_medialib_entry_property_set_int (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION, 
										  (gint) data->total_samples / data->sample_rate * 1000);

	xmms_medialib_entry_property_set_int (session, entry, XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE, 
										  data->sample_rate);

	xmms_medialib_end (session);

	xmms_medialib_entry_send_update (entry);
}

static gboolean
xmms_flac_decode_block (xmms_decoder_t *decoder)
{
	xmms_flac_data_t *data;
	gboolean ret;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	ret = FLAC__seekable_stream_decoder_process_single (data->flacdecoder);

	if (FLAC__seekable_stream_decoder_get_state (data->flacdecoder) == FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM)
		return FALSE;

	return ret;
}

static gboolean
xmms_flac_seek (xmms_decoder_t *decoder, guint samples)
{
	xmms_flac_data_t *data;
	FLAC__bool res;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	data->is_seeking = TRUE;
	res = FLAC__seekable_stream_decoder_seek_absolute (data->flacdecoder, (FLAC__uint64) samples);
	data->is_seeking = FALSE;

	return res;
}

void
xmms_flac_destroy (xmms_decoder_t *decoder)
{
	xmms_flac_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	if (data->vorbiscomment)
		FLAC__metadata_object_delete (data->vorbiscomment);

	FLAC__seekable_stream_decoder_finish (data->flacdecoder);
	FLAC__seekable_stream_decoder_delete (data->flacdecoder);

	g_free (data);
}
