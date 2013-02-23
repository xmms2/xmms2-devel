/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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
#include "xmms/xmms_config.h"
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

	gint16 *samples_buf;
	gint16 *samples_start;
	gint samples_count;
} xmms_speex_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_speex_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_speex_init (xmms_xform_t *xform);
static gint xmms_speex_read (xmms_xform_t *xform, gpointer buf, gint len, xmms_error_t *err);
static void xmms_speex_destroy (xmms_xform_t *xform);
static void xmms_speex_read_metadata (xmms_xform_t *xform, xmms_speex_data_t *data);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("speex",
                   "Speex Decoder", XMMS_VERSION,
                   "Speex decoder",
                   xmms_speex_plugin_setup);

static gboolean
xmms_speex_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_speex_init;
	methods.destroy = xmms_speex_destroy;
	methods.read = xmms_speex_read;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-speex",
	                              NULL);

	xmms_magic_add ("ogg/speex header", "audio/x-speex",
	                "0 string OggS", ">4 byte 0",
	                ">>28 string Speex   ", NULL);

	xmms_magic_extension_add ("audio/x-speex", "*.spx");

	xmms_xform_plugin_config_property_register (xform_plugin,
	                                            "perceptual_enhancer",
	                                            "1", NULL, NULL);

	return TRUE;
}

static gboolean
xmms_speex_init (xmms_xform_t *xform)
{
	gint pe;

	xmms_config_property_t *val;
	xmms_speex_data_t *data;
	xmms_error_t error;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_speex_data_t, 1);
	g_return_val_if_fail (data, FALSE);

	xmms_xform_private_data_set (xform, data);

	ogg_sync_init (&data->sync_state);
	speex_bits_init (&data->speex_bits);

	/* Find the speex header */

	while (42) {
		gint ret;

		data->ogg_data = ogg_sync_buffer (&data->sync_state, 1024);
		ret = xmms_xform_read (xform, data->ogg_data, 1024, &error);
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

	data->speexheader = speex_packet_to_header ((char *)data->ogg_packet.packet,
	                                            data->ogg_packet.bytes);
	data->speex_state = speex_decoder_init (speex_mode_list[data->speexheader->mode]);

	val = xmms_xform_config_lookup (xform, "perceptual_enhancer");
	pe = xmms_config_property_get_int (val);
	speex_decoder_ctl (data->speex_state, SPEEX_SET_ENH, &pe);

	ogg_sync_pageout (&data->sync_state, &data->ogg_page);
	ogg_stream_pagein (&data->stream_state, &data->ogg_page);
	ogg_stream_packetout (&data->stream_state, &data->ogg_packet);

	data->samples_buf = g_new (gint16,
	                           data->speexheader->frames_per_packet *
	                           data->speexheader->frame_size *
	                           data->speexheader->nb_channels);
	data->samples_start = data->samples_buf;
	data->samples_count = 0;

	xmms_speex_read_metadata (xform, data);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->speexheader->nb_channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->speexheader->rate,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static gint
xmms_speex_read (xmms_xform_t *xform, gpointer buf, gint len,
                 xmms_error_t *err)
{
	gint ret = 0, n;
	gfloat outfloat [2000];
	gint16 *outbuf = (gint16 *) buf;
	xmms_speex_data_t *data;
	xmms_error_t error;
	SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT;

	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	/* convert from bytes to samples */
	len /= 2;

	/* first, copy already decoded samples over if we have any. */
	if (data->samples_count) {
		n = MIN (data->samples_count, len);

		memcpy (outbuf, data->samples_start, n * 2);
		data->samples_count -= n;

		if (!data->samples_count) {
			data->samples_start = data->samples_buf;
		} else {
			data->samples_start += n;
		}

		/* convert from samples to bytes */
		return n * 2;
	}

	while (42) {
		gint samples_per_frame;

		samples_per_frame = data->speexheader->frame_size *
		                    data->speexheader->nb_channels;

		while (ogg_stream_packetout (&data->stream_state, &data->ogg_packet) == 1) {
			gint frame;

			speex_bits_read_from (&data->speex_bits,
			                      (char *)data->ogg_packet.packet,
			                      data->ogg_packet.bytes);

			for (frame = 0; frame < data->speexheader->frames_per_packet; frame++) {
				gint cnt;

				speex_decode (data->speex_state, &data->speex_bits, outfloat);

				if (data->speexheader->nb_channels == 2) {
					speex_decode_stereo (outfloat, data->speexheader->frame_size,&stereo);
				}

				n = MIN (samples_per_frame, len);

				/* copy as many samples to the output buffer as
				 * possible.
				 */
				for (cnt = 0; cnt < n; cnt++) {
					*outbuf++ = outfloat[cnt];
					len--;
					ret += 2;
				}

				/* store the remaining samples for later use */
				for (; cnt < samples_per_frame; cnt++) {
					data->samples_buf[data->samples_count++] =
						outfloat[cnt];
				}
			}

			return ret;
		}

		/* Need more data */

		do {
			gint ret;

			data->ogg_data = ogg_sync_buffer (&data->sync_state, 200);
			ret = xmms_xform_read (xform, data->ogg_data, 200, &error);
			ogg_sync_wrote (&data->sync_state, ret);

			if (ret <= 0) {
				return ret;
			}
		} while (ogg_sync_pageout (&data->sync_state, &data->ogg_page) != 1);

		ogg_stream_pagein (&data->stream_state, &data->ogg_page);
	}
}

static void
xmms_speex_destroy (xmms_xform_t *xform)
{
	xmms_speex_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	ogg_stream_clear (&data->stream_state);
	ogg_sync_clear (&data->sync_state);

	speex_decoder_destroy (data->speex_state);
	speex_bits_destroy (&data->speex_bits);

	g_free (data->samples_buf);
	g_free (data->speexheader);
	g_free (data);
}

static void
xmms_speex_read_metadata (xmms_xform_t *xform, xmms_speex_data_t *data)
{
	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
	                             data->speexheader->bitrate);
}

