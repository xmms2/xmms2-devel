/** @file faad.c
 *  Decoder plugin for AAC and MP4 audio formats
 *
 *  Copyright (C) 2005 Juho Vähä-Herttua, <juhovh@iki.fi>
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
#include "xmms/xmms_transport.h"
#include "xmms/xmms_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <faad.h>
#include <glib.h>

#include "mp4ff/mp4ff.h"

#define FAAD_BUFFER_SIZE 4096

#define FAAD_TYPE_UNKNOWN 0
#define FAAD_TYPE_MP4 1
#define FAAD_TYPE_ADIF 2
#define FAAD_TYPE_ADTS 3

static int faad_mpeg_samplerates[] = { 96000, 88200, 64000, 48000, 44100, 
                                       32000, 24000, 22050, 16000, 12000, 
                                       11025, 8000, 7350, 0, 0, 0 };

typedef struct {
	faacDecHandle decoder;
	gint filetype;

	mp4ff_t *mp4ff;
	mp4ff_callback_t *mp4ff_cb;
	gint track;
	glong sampleid;
	glong numsamples;
	gint toskip;

	guchar buffer[FAAD_BUFFER_SIZE];
	guint buffer_length;
	guint buffer_size;

	guint channels;
	guint bitrate;
	guint samplerate;
	xmms_sample_format_t sampleformat;
} xmms_faad_data_t;

static gboolean xmms_faad_new (xmms_decoder_t *decoder);
static void xmms_faad_destroy (xmms_decoder_t *decoder);
static gboolean xmms_faad_init (xmms_decoder_t *decoder, gint mode);
static gboolean xmms_faad_decode_block (xmms_decoder_t *decoder);
static gboolean xmms_faad_seek (xmms_decoder_t *decoder, guint samples);
static void xmms_faad_get_mediainfo (xmms_decoder_t *decoder);

uint32_t xmms_faad_read_callback (void *user_data, void *buffer,
				  uint32_t length);
uint32_t xmms_faad_seek_callback (void *user_data, uint64_t position);
int xmms_faad_get_aac_track (mp4ff_t * infile);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER,
	                          XMMS_DECODER_PLUGIN_API_VERSION, "faad",
	                          "AAC Decoder", XMMS_VERSION, "AAC decoder");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.audiocoding.com/");
	xmms_plugin_info_add (plugin, "Author", "juhovh");
	xmms_plugin_info_add (plugin, "License", "GPL");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_faad_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY,
	                        xmms_faad_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_faad_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK,
	                        xmms_faad_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_faad_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO,
	                        xmms_faad_get_mediainfo);

	xmms_plugin_magic_add (plugin, "mpeg-4 header", "video/mp4",
	                       "4 string ftyp",
	                       ">8 string isom",
	                       ">8 string mp41",
	                       ">8 string mp42",
	                       NULL);

	xmms_plugin_magic_add (plugin, "iTunes mpeg-4 header", "audio/mp4",
	                       "4 string ftyp", ">8 string M4A ", NULL);

	xmms_plugin_magic_add (plugin, "mpeg aac header", "audio/aac",
	                       "0 beshort&0xfff6 0xfff0", NULL);

	xmms_plugin_magic_add (plugin, "adif header", "audio/aac",
	                       "0 string ADIF", NULL);

	return plugin;
}

static gboolean
xmms_faad_new (xmms_decoder_t *decoder)
{
	xmms_faad_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = g_new0 (xmms_faad_data_t, 1);
	data->buffer_size = FAAD_BUFFER_SIZE;

	xmms_decoder_private_data_set (decoder, data);

	return TRUE;
}

static void
xmms_faad_destroy (xmms_decoder_t *decoder)
{
	xmms_faad_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	faacDecClose (data->decoder);
	if (data->mp4ff) {
		mp4ff_close (data->mp4ff);
	}
	g_free (data->mp4ff_cb);

	g_free (data);
}

static gboolean
xmms_faad_init (xmms_decoder_t *decoder, gint mode)
{
	xmms_faad_data_t *data;
	xmms_transport_t *transport;
	xmms_error_t error;

	faacDecConfigurationPtr config;
	gint bytes_read;
	gulong samplerate;
	guchar channels;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	data->sampleid = 0;
	data->numsamples = 0;
	data->decoder = faacDecOpen ();
	config = faacDecGetCurrentConfiguration (data->decoder);
	config->defObjectType = LC;
	config->defSampleRate = 44100;
	config->outputFormat = FAAD_FMT_16BIT;
	config->downMatrix = 0;
	config->dontUpSampleImplicitSBR = 0;
	faacDecSetConfiguration (data->decoder, config);

	switch (config->outputFormat) {
	case FAAD_FMT_16BIT:
		data->sampleformat = XMMS_SAMPLE_FORMAT_S16;
		break;
	case FAAD_FMT_24BIT:
		/* we don't have 24-bit format to use in xmms2 */
		data->sampleformat = XMMS_SAMPLE_FORMAT_S32;
		break;
	case FAAD_FMT_32BIT:
		data->sampleformat = XMMS_SAMPLE_FORMAT_S32;
		break;
	case FAAD_FMT_FLOAT:
		data->sampleformat = XMMS_SAMPLE_FORMAT_FLOAT;
		break;
	case FAAD_FMT_DOUBLE:
		data->sampleformat = XMMS_SAMPLE_FORMAT_DOUBLE;
		break;
	}

	bytes_read = xmms_transport_read (transport,
	                                  (gchar *) data->buffer + data->buffer_length,
	                                  data->buffer_size - data->buffer_length,
	                                  &error);

	data->buffer_length += bytes_read;

	if (bytes_read < 8) {
		XMMS_DBG ("Not enough bytes to check the MP4 header");
		return FALSE;
	}

	/* which type of file are we dealing with? */
	data->filetype = FAAD_TYPE_UNKNOWN;
	if (!strncmp ((char *)&data->buffer[4], "ftyp", 4)) {
		data->filetype = FAAD_TYPE_MP4;
	} else if (!strncmp ((char *)data->buffer, "ADIF", 4)) {
		data->filetype = FAAD_TYPE_ADIF;
	} else {
		int i;

		/* ADTS mpeg file can be a stream and start in the middle of a
		 * frame so we need to have extra loop check here */
		for (i=0; i<data->buffer_length-1; i++) {
			if (data->buffer[i] == 0xff && (data->buffer[i+1]&0xf6) == 0xf0) {
				data->filetype = FAAD_TYPE_ADTS;
				g_memmove (data->buffer, data->buffer+i, data->buffer_length-i);
				data->buffer_length -= i;
				break;
			}
		}
	}

	if (data->filetype == FAAD_TYPE_MP4) {
		guchar *tmpbuf;
		guint tmpbuflen;

		/*
		 * MP4 not supported (yet) on non-seekable transport
		 * this needs little tweaking in mp4ff at least
		 */
		if (!xmms_transport_can_seek (transport)) {
			XMMS_DBG ("Non-seekable transport on MP4 not yet supported");
			return FALSE;
		}

		data->mp4ff_cb = g_new0 (mp4ff_callback_t, 1);
		data->mp4ff_cb->read = xmms_faad_read_callback;
		data->mp4ff_cb->seek = xmms_faad_seek_callback;
		data->mp4ff_cb->user_data = decoder;

		data->mp4ff = mp4ff_open_read (data->mp4ff_cb);
		if (!data->mp4ff) {
			XMMS_DBG ("Error opening mp4 demuxer\n");
			return FALSE;
		}

		data->track = xmms_faad_get_aac_track (data->mp4ff);
		if (data->track < 0) {
			XMMS_DBG ("Can't find AAC audio track from MP4 file\n");
			return FALSE;
		}
		data->numsamples = mp4ff_num_samples(data->mp4ff, data->track);
		mp4ff_get_decoder_config (data->mp4ff, data->track, &tmpbuf,
					  &tmpbuflen);

		if (faacDecInit2 (data->decoder, tmpbuf, tmpbuflen,
		                  &samplerate, &channels) < 0) {
			XMMS_DBG ("Error initializing decoder library.");
			g_free (tmpbuf);
			return FALSE;
		}
		g_free (tmpbuf);

		data->samplerate = samplerate;
		data->channels = channels;
	} else if (data->filetype == FAAD_TYPE_ADTS || data->filetype == FAAD_TYPE_ADIF) {
		if ((bytes_read =
		     faacDecInit (data->decoder, data->buffer, data->buffer_length,
		                  &samplerate, &channels)) < 0) {
			XMMS_DBG ("Error initializing decoder library.");
			return FALSE;
		}

		/* If we are not checking mediainfo, skip the possible header */
		if (mode ^ XMMS_DECODER_INIT_MEDIAINFO) {
			g_memmove (data->buffer, data->buffer + bytes_read,
			           data->buffer_length - bytes_read);
			data->buffer_length -= bytes_read;
		}

		data->samplerate = samplerate;
		data->channels = channels;
	}

	if (mode & XMMS_DECODER_INIT_DECODING) {
		xmms_decoder_format_add (decoder, data->sampleformat,
		                         data->channels, data->samplerate);
		if (xmms_decoder_format_finish (decoder) == NULL) {
			XMMS_DBG ("Decoder format finish failed!");
			return FALSE;
		}
		XMMS_DBG ("AAC decoder inited successfully!");
	}

	return TRUE;
}

static gboolean
xmms_faad_decode_block (xmms_decoder_t *decoder)
{
	xmms_faad_data_t *data;
	xmms_transport_t *transport;
	xmms_error_t error;

	faacDecFrameInfo frameInfo;
	gpointer sample_buffer;
	guint bytes_read = 0;

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	if (data->filetype == FAAD_TYPE_MP4) {
		guchar *tmpbuf;
		guint tmpbuflen;

		bytes_read = mp4ff_read_sample (data->mp4ff, data->track, 
		                                data->sampleid, &tmpbuf, 
		                                &tmpbuflen);
		data->sampleid++;

		if (bytes_read <= 0 || data->sampleid >= data->numsamples) {
			XMMS_DBG ("MP4 EOF");
			return FALSE;
		}

		sample_buffer = faacDecDecode (data->decoder, &frameInfo, 
		                               tmpbuf, tmpbuflen);
		bytes_read = frameInfo.samples * xmms_sample_size_get (data->sampleformat);
		g_free (tmpbuf);
	} else if (data->filetype == FAAD_TYPE_ADTS || data->filetype == FAAD_TYPE_ADIF) {
		if (data->buffer_length < FAAD_BUFFER_SIZE) {
			bytes_read = xmms_transport_read (transport,
							  (gchar *) data->buffer + data->buffer_length,
							  data->buffer_size - data->buffer_length,
							  &error);

			if (bytes_read <= 0 && data->buffer_length == 0) {
				XMMS_DBG ("EOF");
				return FALSE;
			}

			data->buffer_length += bytes_read;
		}

		sample_buffer = faacDecDecode (data->decoder, &frameInfo, data->buffer,
		                               data->buffer_length);

		g_memmove (data->buffer, data->buffer + frameInfo.bytesconsumed,
		           data->buffer_length - frameInfo.bytesconsumed);
		data->buffer_length -= frameInfo.bytesconsumed;
		bytes_read = frameInfo.samples * xmms_sample_size_get (data->sampleformat);
	}

	if (bytes_read > 0 && frameInfo.error == 0) {
		xmms_decoder_write (decoder, sample_buffer + data->toskip,
		                    bytes_read - data->toskip);
		data->toskip = 0;
	} else if (frameInfo.error > 0) {
		XMMS_DBG ("ERROR in faad decoding: %s", faacDecGetErrorMessage(frameInfo.error));
		return FALSE;
	}

	return TRUE;
}

static gboolean
xmms_faad_seek (xmms_decoder_t *decoder, guint samples)
{
	xmms_faad_data_t *data;
	xmms_transport_t *transport;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	if (data->filetype == FAAD_TYPE_MP4) {
		int32_t toskip;

		data->sampleid = mp4ff_find_sample (data->mp4ff, data->track, 
		                                    samples, &toskip);
		data->toskip = toskip * data->channels * xmms_sample_size_get (data->sampleformat);
		data->buffer_length = 0;

		return TRUE;
	}

	/* Seeking not supported on non-MP4 AAC right now */
	return FALSE;
}

static void
xmms_faad_get_mediainfo (xmms_decoder_t *decoder)
{
	xmms_faad_data_t *data;
	xmms_transport_t *transport;
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	transport = xmms_decoder_transport_get (decoder);
	g_return_if_fail (transport);

	entry = xmms_decoder_medialib_entry_get (decoder);
	session = xmms_medialib_begin_write ();

	if (data->filetype == FAAD_TYPE_MP4) {
		glong temp;
		gchar *metabuf;

		temp = mp4ff_get_sample_rate (data->mp4ff, data->track);
		xmms_medialib_entry_property_set_int (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE,
		                                      temp);
		if ((temp = mp4ff_get_track_duration (data->mp4ff, data->track) / temp) >= 0) {
			xmms_medialib_entry_property_set_int (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			                                      temp * 1000);
		}
		if ((temp = mp4ff_get_avg_bitrate (data->mp4ff, data->track)) >= 0) {
			xmms_medialib_entry_property_set_int (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
			                                      temp);
		}
		if (mp4ff_meta_get_artist (data->mp4ff, &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,
			                                      metabuf);
			g_free (metabuf);
		}
		if (mp4ff_meta_get_title (data->mp4ff, &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,
			                                      metabuf);
			g_free (metabuf);
		}
		if (mp4ff_meta_get_album (data->mp4ff, &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,
			                                      metabuf);
			g_free (metabuf);
		}
		if (mp4ff_meta_get_date (data->mp4ff, &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,
			                                      metabuf);
			g_free (metabuf);
		}
		if (mp4ff_meta_get_genre (data->mp4ff, &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,
			                                      metabuf);
			g_free (metabuf);
		}
		if (mp4ff_meta_get_comment (data->mp4ff, &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT,
			                                      metabuf);
			g_free (metabuf);
		}
		if (mp4ff_meta_get_track (data->mp4ff, &metabuf)) {
			gint tracknr;
			gchar *end;

			tracknr = strtol(metabuf, &end, 10);
			if (end && *end == '\0') {
				xmms_medialib_entry_property_set_int (session, entry,
				                                      XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,
				                                      tracknr);
			}
			g_free (metabuf);
		}

		/* MusicBrainz tag support */
		if (mp4ff_meta_find_by_name (data->mp4ff, "MusicBrainz Track Id", &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID,
							      metabuf);
			g_free (metabuf);
		}
		if (mp4ff_meta_find_by_name (data->mp4ff, "MusicBrainz Album Id", &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID,
							      metabuf);
			g_free (metabuf);
		}
		if (mp4ff_meta_find_by_name (data->mp4ff, "MusicBrainz Artist Id", &metabuf)) {
			xmms_medialib_entry_property_set_str (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID,
							      metabuf);
			g_free (metabuf);
		}
	} else if (data->filetype == FAAD_TYPE_ADIF) {
		guint skip_size, bitrate;
		guint64 duration;

		skip_size = (data->buffer[4] & 0x80) ? 9 : 0;
		bitrate = ((guint) (data->buffer[4 + skip_size] & 0x0F) << 19) |
		          ((guint) data->buffer[5 + skip_size] << 11) |
		          ((guint) data->buffer[6 + skip_size] << 3) |
		          ((guint) data->buffer[7 + skip_size] & 0xE0);
		xmms_medialib_entry_property_set_int (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
		                                      bitrate);

		duration = xmms_transport_size (transport);
		if (duration != 0) {
			duration = ((float) duration * 8000.f) / ((float) bitrate) + 0.5f;
			xmms_medialib_entry_property_set_int (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			                                      duration);
		}
	} else if (data->filetype == FAAD_TYPE_ADTS) {
		xmms_medialib_entry_property_set_int (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE,
		                                      faad_mpeg_samplerates[(data->buffer[2]&0x3c)>>2]);
	}

	xmms_medialib_end (session);
	xmms_medialib_entry_send_update (entry);
}

uint32_t
xmms_faad_read_callback (void *user_data, void *buffer, uint32_t length)
{
	xmms_decoder_t *decoder;
	xmms_faad_data_t *data;
	xmms_transport_t *transport;
	xmms_error_t error;
	gint ret;

	g_return_val_if_fail (user_data, 0);
	g_return_val_if_fail (buffer, 0);
	decoder = user_data;

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, 0);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, 0);

	if (data->buffer_length == 0) {
		guint bytes_read;

		bytes_read = xmms_transport_read (transport, (gchar *) data->buffer, 
		                                  data->buffer_size, &error);

		if (bytes_read <= 0 && data->buffer_length == 0) {
			return bytes_read;
		}

		data->buffer_length += bytes_read;
	}

	ret = MIN (length, data->buffer_length);
	g_memmove (buffer, data->buffer, ret);
	g_memmove (data->buffer, data->buffer + ret, data->buffer_length - ret);
	data->buffer_length -= ret;

	return ret;
}

uint32_t
xmms_faad_seek_callback (void *user_data, uint64_t position)
{
	xmms_decoder_t *decoder;
	xmms_faad_data_t *data;
	xmms_transport_t *transport;
	gint ret = 0;

	g_return_val_if_fail (user_data, -1);
	decoder = user_data;

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, -1);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, -1);

	if (!xmms_transport_can_seek (transport)) {
		return -1;
	}

	ret = xmms_transport_seek (transport, position,
	                           XMMS_TRANSPORT_SEEK_SET);
	data->buffer_length = 0;

	return ret;
}

int
xmms_faad_get_aac_track (mp4ff_t *infile)
{
	/* find first AAC audio track */
	int i, rc;
	int numTracks = mp4ff_total_tracks (infile);

	for (i = 0; i < numTracks; i++) {
		guchar *buff = NULL;
		guint buff_size = 0;
		mp4AudioSpecificConfig mp4ASC;

		mp4ff_get_decoder_config (infile, i, &buff, &buff_size);

		if (buff) {
			rc = AudioSpecificConfig (buff, buff_size, &mp4ASC);
			g_free (buff);

			if (rc < 0) {
				continue;
			}
			return i;
		}
	}

	/* can't decode this */
	return -1;
}
