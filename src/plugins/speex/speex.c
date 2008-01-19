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

#include "xmms/xmms_decoderplugin.h"
#include "xmms/xmms_log.h"

#include <string.h>
#include <math.h>

#include <glib.h>
#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#include <ogg/ogg.h>

typedef struct xmms_speex_data_St {
	void *speex_state;
	SpeexBits speex_bits;
	SpeexHeader *speexheader;

	ogg_sync_state sync_state;
	ogg_stream_state stream_state;
	ogg_page ogg_page;
	ogg_packet ogg_packet;
	gchar *ogg_data;
} xmms_speex_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_speex_new (xmms_decoder_t *decoder);
static gboolean xmms_speex_init (xmms_decoder_t *decoder, gint mode);
static gboolean xmms_speex_seek (xmms_decoder_t *decoder, guint samples);
static gboolean xmms_speex_decode_block (xmms_decoder_t *decoder);
static void xmms_speex_destroy (xmms_decoder_t *decoder);
static void xmms_speex_get_mediainfo (xmms_decoder_t *decoder);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER,
	                          XMMS_DECODER_PLUGIN_API_VERSION,
	                          "speex",
	                          "Speex Decoder",
	                          XMMS_VERSION,
	                          "Speex decoder");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.speex.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_speex_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_speex_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_speex_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_speex_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_speex_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_speex_get_mediainfo);

	//xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	//xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	xmms_plugin_config_property_register (plugin, "perceptual_enhancer", "1",
	                                      NULL, NULL);

	xmms_plugin_magic_add (plugin, "ogg/speex header", "audio/x-speex",
	                       "0 string OggS", ">4 byte 0",
	                       ">>28 string Speex   ", NULL);

	return plugin;
}

static gboolean
xmms_speex_new (xmms_decoder_t *decoder)
{
	xmms_speex_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = g_new0 (xmms_speex_data_t, 1);

	xmms_decoder_private_data_set (decoder, data);

	return TRUE;
}

static gboolean
xmms_speex_init (xmms_decoder_t *decoder, gint mode)
{
	gint pe;

	xmms_config_property_t *val;
	xmms_speex_data_t *data;
	xmms_error_t error;
	xmms_transport_t *transport;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	ogg_sync_init (&data->sync_state);
	speex_bits_init (&data->speex_bits);

	/* Find the speex header */

	while (42) {
		gint ret;

		data->ogg_data = ogg_sync_buffer (&data->sync_state, 1024);
		ret = xmms_transport_read (transport, data->ogg_data, 1024, &error);
		ogg_sync_wrote (&data->sync_state, ret);

		if (ret <= 0) {
			return FALSE;
		}

		if (ogg_sync_pageout (&data->sync_state, &data->ogg_page) == 1) {
			break;
		}
	}

	ogg_stream_init (&data->stream_state, ogg_page_serialno (&data->ogg_page));

	if (ogg_stream_pagein (&data->stream_state, &data->ogg_page) < 0) {
		return FALSE;
	}

	if (ogg_stream_packetout (&data->stream_state, &data->ogg_packet) != 1) {
		return FALSE;
	}

	data->speexheader = speex_packet_to_header (data->ogg_packet.packet,
	                                            data->ogg_packet.bytes);
	data->speex_state = speex_decoder_init (speex_mode_list[data->speexheader->mode]);

	val = xmms_plugin_config_lookup (xmms_decoder_plugin_get (decoder),
	                                 "perceptual_enhancer");
	pe = xmms_config_property_get_int (val);
	speex_decoder_ctl (data->speex_state, SPEEX_SET_ENH, &pe);

	ogg_sync_pageout (&data->sync_state, &data->ogg_page);
	ogg_stream_pagein (&data->stream_state, &data->ogg_page);
	ogg_stream_packetout (&data->stream_state, &data->ogg_packet);

	xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_S16,
	                         data->speexheader->nb_channels,
	                         data->speexheader->rate);
	/* we don't have to care about the return value other than NULL,
	   as there is only one format (to rule them all) */
	if (xmms_decoder_format_finish (decoder) == NULL) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
xmms_speex_seek (xmms_decoder_t *decoder, guint samples)
{
	g_return_val_if_fail (decoder, FALSE);

	return FALSE; /* Seeking not supported right now */
}

static gboolean
xmms_speex_decode_block (xmms_decoder_t *decoder)
{
	gint ret;
	gfloat outfloat [2000];
	gint16 outshort [2000];
	xmms_speex_data_t *data;
	xmms_error_t error;
	xmms_transport_t *transport;
	SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	if (ogg_stream_packetout (&data->stream_state, &data->ogg_packet) > 0) {
		gint frame, cnt;

		speex_bits_read_from (&data->speex_bits,
		                      data->ogg_packet.packet,
		                      data->ogg_packet.bytes);

		for (frame = 0; frame < data->speexheader->frames_per_packet; frame++) {

			speex_decode (data->speex_state, &data->speex_bits, outfloat);

			if (data->speexheader->nb_channels == 2) {
				speex_decode_stereo (outfloat, data->speexheader->frame_size,&stereo);
			}

			for (cnt = 0; cnt < data->speexheader->frame_size *
			                    data->speexheader->nb_channels; cnt++) {
				outshort [cnt] = outfloat [cnt];
			}

			xmms_decoder_write (decoder, (gchar *) outshort,
			                    data->speexheader->frame_size *
			                    data->speexheader->nb_channels * 2);
		}

		return TRUE;
	}

	/* Need more data */

	data->ogg_data = ogg_sync_buffer (&data->sync_state, 200);
	ret = xmms_transport_read (transport, data->ogg_data, 200, &error);
	ogg_sync_wrote (&data->sync_state, ret);

	if ( ret <= 0 ) {
		return FALSE;
	}

	if (ogg_sync_pageout (&data->sync_state, &data->ogg_page) < 1) {
		return TRUE;
	}

	if (ogg_stream_pagein (&data->stream_state, &data->ogg_page) != 0) {
		return TRUE;
	}

	return TRUE;
}

static void
xmms_speex_destroy (xmms_decoder_t *decoder)
{
	xmms_speex_data_t *data;

	g_return_if_fail (decoder);
	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	ogg_stream_clear (&data->stream_state);
	ogg_sync_clear (&data->sync_state);

	speex_decoder_destroy (data->speex_state);
	speex_bits_destroy (&data->speex_bits);

	g_free (data->speexheader);
	g_free (data);
}

static void
xmms_speex_get_mediainfo (xmms_decoder_t *decoder)
{
	xmms_medialib_entry_t entry;
	xmms_speex_data_t *data;

	g_return_if_fail (decoder);

	if (!xmms_speex_init (decoder, 0))
		return;

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	entry = xmms_medialib_entry_new (NULL);

	xmms_medialib_entry_property_set_int (entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
	                                      data->speexheader->bitrate);

	xmms_medialib_entry_send_update (entry);
}

