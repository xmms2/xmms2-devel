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



#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"
#include "xmms/xmms.h"

#include <string.h>
#include <math.h>
#include <FLAC/all.h>

#include <glib.h>

#warning "CONVERT TO SAMPLE_T"

typedef struct xmms_flac_data_St {
	FLAC__SeekableStreamDecoder *flacdecoder;
	FLAC__StreamMetadata *vorbiscomment;
	guint channels;
	guint sample_rate;
	guint bits_per_sample;
	guint64 total_samples;
	gboolean inited;
} xmms_flac_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_flac_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_flac_init (xmms_decoder_t *decoder);
static gboolean xmms_flac_seek (xmms_decoder_t *decoder, guint samples);
static gboolean xmms_flac_can_handle (const gchar *mimetype);
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

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "flac",
			"FLAC decoder " XMMS_VERSION,
			"Free Lossless Audio Codec decoder");

	xmms_plugin_info_add (plugin, "URL", "http://flac.sourceforge.net/");
	xmms_plugin_info_add (plugin, "Author", "Michael Lindgren");
	xmms_plugin_info_add (plugin, "E-Mail", "lindgren@debian.as");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_flac_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_flac_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_flac_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_flac_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_flac_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_flac_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_flac_get_mediainfo);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	return plugin;
}

static gboolean
xmms_flac_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);

	if ((g_strcasecmp (mimetype, "audio/x-flac") == 0))
		return TRUE;

	if ((g_strcasecmp (mimetype, "application/x-flac") == 0))
		return TRUE;

	return FALSE;
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

	ret = xmms_transport_read (transport, buffer, *bytes, &error);
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
	xmms_transport_t *transport;
	FLAC__int16	*pcmdata;
	guint cur_sample, cur_channel, bufpos = 0;

	guint sample_rate;
	guint samples;

	transport = xmms_decoder_transport_get (decoder);
	data = xmms_decoder_private_data_get (decoder);

	sample_rate	= data->sample_rate;
	samples	= frame->header.blocksize;

	pcmdata = g_malloc0 (samples * data->channels * sizeof (FLAC__int16));

	for (cur_sample = 0; cur_sample < samples; cur_sample++) {
		for (cur_channel = 0; cur_channel < data->channels; cur_channel++) {
			pcmdata[bufpos] = (FLAC__int16) buffer[cur_channel][cur_sample];
			bufpos += 1;
		}
	}

	xmms_decoder_write (decoder, (gchar *) pcmdata, samples * data->channels * sizeof (FLAC__int16));

	g_free (pcmdata);

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
	xmms_flac_data_t *data;

	data = xmms_decoder_private_data_get (decoder);

	switch (metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:	/* FLAC__metadata_object_clone ()? */
			data->bits_per_sample = metadata->data.stream_info.bits_per_sample;
			data->sample_rate = metadata->data.stream_info.sample_rate;
			data->channels = metadata->data.stream_info.channels;
			data->total_samples = metadata->data.stream_info.total_samples;
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
xmms_flac_new (xmms_decoder_t *decoder, const gchar *mimetype)
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
xmms_flac_init (xmms_decoder_t *decoder)
{
	xmms_flac_data_t *data;
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

	data->inited = TRUE;

	xmms_decoder_samplerate_set (decoder, data->sample_rate);

	return TRUE;
}

static void
xmms_flac_get_mediainfo (xmms_decoder_t *decoder)
{
	xmms_flac_data_t *data;
	xmms_playlist_entry_t *entry;
	gint current, num_comments;
	gchar tmp[20];

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	if (!data->inited)
		xmms_flac_init (decoder);

	entry = xmms_playlist_entry_new (NULL);

	XMMS_DBG ("Vendorstring: %s", data->vorbiscomment->data.vorbis_comment.vendor_string.entry); /* hehehe */

	num_comments = data->vorbiscomment->data.vorbis_comment.num_comments;

	for (current = 0; current < num_comments; current++) {
		gchar **s, *val;
		guint length;

		s = g_strsplit (data->vorbiscomment->data.vorbis_comment.comments[current].entry, "=", 2);
		length = data->vorbiscomment->data.vorbis_comment.comments[current].length - strlen (s[0]) - 1;

		val = g_strndup (s[1], length);
		xmms_playlist_entry_property_set (entry, s[0], val);
		XMMS_DBG ("Setting %s to %s", s[0], val);
		g_free (val);

		g_strfreev (s);
	}

	g_snprintf (tmp, sizeof (tmp), "%d", (gint) data->bits_per_sample * data->sample_rate);
	xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE, tmp);

	g_snprintf (tmp, sizeof (tmp), "%d", (gint) data->total_samples / data->sample_rate * 1000);
	xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, tmp);

	g_snprintf (tmp, sizeof (tmp), "%d", data->sample_rate);
	xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_SAMPLERATE, tmp);

	xmms_decoder_entry_mediainfo_set (decoder, entry);
	xmms_object_unref (entry);
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

	res = FLAC__seekable_stream_decoder_seek_absolute (data->flacdecoder, (FLAC__uint64) samples);

	return res;
}

void
xmms_flac_destroy (xmms_decoder_t *decoder)
{
	xmms_flac_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	FLAC__metadata_object_delete (data->vorbiscomment);
	FLAC__seekable_stream_decoder_finish (data->flacdecoder);
	FLAC__seekable_stream_decoder_delete (data->flacdecoder);

	g_free (data);
}
