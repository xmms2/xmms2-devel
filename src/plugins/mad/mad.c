/*  XMMS2 - X Music Multiplexer System
 *
 *  Copyright (C) 2003-2023 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */




/** @file
 * MPEG Layer 1/2/3 decoder plugin.
 *
 * Supports Xing VBR and id3v1/v2
 *
 * This code basicly sucks at some places. But hey, the
 * standard is fucked. Please convert to Ogg ;-)
 */


#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_log.h>
#include "xing.h"
#include <mad.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "../mp3_common/id3v1.c"

/*
 * Type definitions
 */

typedef struct xmms_mad_data_St {
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;

	guchar buffer[4096];
	guint buffer_length;
	guint channels;
	guint bitrate;
	guint samplerate;
	guint64 fsize;

	guint synthpos;
	gint samples_to_skip;
	gint64 samples_to_play;
	gint frames_to_skip;

	xmms_xing_t *xing;
} xmms_mad_data_t;


/*
 * Function prototypes
 */

static gboolean xmms_mad_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_mad_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static void xmms_mad_destroy (xmms_xform_t *decoder);
static gboolean xmms_mad_init (xmms_xform_t *decoder);
static gint64 xmms_mad_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("mad",
                          "MAD decoder",
                          XMMS_VERSION,
                          "MPEG Layer 1/2/3 decoder",
                          xmms_mad_plugin_setup);

static gboolean
xmms_mad_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_mad_init;
	methods.destroy = xmms_mad_destroy;
	methods.read = xmms_mad_read;
	methods.seek = xmms_mad_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	  xmms_plugin_info_add (plugin, "URL", "http://xmms2.org/");
	  xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	  xmms_plugin_info_add (plugin, "License", "GPL");
	*/

	xmms_xform_plugin_config_property_register (xform_plugin, "id3v1_encoding",
	                                            "ISO8859-1", NULL, NULL);

	xmms_xform_plugin_config_property_register (xform_plugin, "id3v1_enable",
	                                            "1", NULL, NULL);

	/* xmms_xform_indata_constraint_add */
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/mpeg",
	                              XMMS_STREAM_TYPE_END);

	xmms_magic_add ("mpeg header", "audio/mpeg",
	                "0 beshort&0xfff6 0xfff6",
	                "0 beshort&0xfff6 0xfff4",
	                "0 beshort&0xffe6 0xffe2",
	                NULL);

	xmms_magic_extension_add ("audio/mpeg", "*.mp3");

	return TRUE;
}

static void
xmms_mad_destroy (xmms_xform_t *xform)
{
	xmms_mad_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	mad_stream_finish (&data->stream);
	mad_frame_finish (&data->frame);
	mad_synth_finish (&data->synth);

	if (data->xing) {
		xmms_xing_free (data->xing);
	}

	g_free (data);

}

static gint64
xmms_mad_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_mad_data_t *data;
	guint bytes;
	gint64 res;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);

	if (data->xing &&
	    xmms_xing_has_flag (data->xing, XMMS_XING_FRAMES) &&
	    xmms_xing_has_flag (data->xing, XMMS_XING_TOC)) {
		guint i;

		i = (guint) (100ULL * samples / xmms_xing_get_frames (data->xing) / 1152);

		bytes = xmms_xing_get_toc (data->xing, i) * (xmms_xing_get_bytes (data->xing) / 256);
	} else {
		bytes = (guint)(((gdouble)samples) * data->bitrate / data->samplerate) / 8;
	}

	XMMS_DBG ("Try seek %" G_GINT64_FORMAT " samples -> %d bytes", samples, bytes);

	res = xmms_xform_seek (xform, bytes, XMMS_XFORM_SEEK_SET, err);
	if (res == -1) {
		return -1;
	}

	/* we don't have sample accuracy when seeking,
	   so there is no use trying */
	data->samples_to_skip = 0;
	data->samples_to_play = -1;

	return samples;
}


static gboolean
xmms_mad_init (xmms_xform_t *xform)
{
	struct mad_frame frame;
	struct mad_stream stream;
	xmms_error_t err;
	guchar buf[40960];
	xmms_mad_data_t *data;
	int len;
	const gchar *metakey;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_mad_data_t, 1);

	mad_stream_init (&data->stream);
	mad_frame_init (&data->frame);
	mad_synth_init (&data->synth);

	xmms_xform_private_data_set (xform, data);

	data->buffer_length = 0;

	data->synthpos = 0x7fffffff;

	mad_stream_init (&stream);
	mad_frame_init (&frame);

	len = xmms_xform_peek (xform, buf, 40960, &err);
	mad_stream_buffer (&stream, buf, len);

	while (mad_frame_decode (&frame, &stream) == -1) {
		if (!MAD_RECOVERABLE (stream.error)) {
			XMMS_DBG ("couldn't decode %02x %02x %02x %02x",buf[0],buf[1],buf[2],buf[3]);
			mad_frame_finish (&frame);
			mad_stream_finish (&stream);
			return FALSE;
		}
	}

	data->channels = frame.header.mode == MAD_MODE_SINGLE_CHANNEL ? 1 : 2;
	data->samplerate = frame.header.samplerate;


	if (frame.header.flags & MAD_FLAG_PROTECTION) {
		XMMS_DBG ("Frame has protection enabled");
		if (stream.anc_ptr.byte > stream.buffer + 2) {
			stream.anc_ptr.byte = stream.anc_ptr.byte - 2;
		}
	}

	data->samples_to_play = -1;

	data->xing = xmms_xing_parse (stream.anc_ptr);
	if (data->xing) {
		xmms_xing_lame_t *lame;
		XMMS_DBG ("File with Xing header!");

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_IS_VBR;
		xmms_xform_metadata_set_int (xform, metakey, 1);

		if (xmms_xing_has_flag (data->xing, XMMS_XING_FRAMES)) {
			guint duration;
			mad_timer_t timer;

			timer = frame.header.duration;
			mad_timer_multiply (&timer, xmms_xing_get_frames (data->xing));
			duration = mad_timer_count (timer, MAD_UNITS_MILLISECONDS);

			XMMS_DBG ("XING duration %d", duration);

			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
			xmms_xform_metadata_set_int (xform, metakey, duration);

			if (xmms_xing_has_flag (data->xing, XMMS_XING_BYTES) && duration) {
				guint tmp;

				tmp = xmms_xing_get_bytes (data->xing) * ((guint64)8000) / duration;
				XMMS_DBG ("XING bitrate %d", tmp);
				metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
				xmms_xform_metadata_set_int (xform, metakey, tmp);
			}
		}

		lame = xmms_xing_get_lame (data->xing);
		if (lame) {
			/* FIXME: add a check for ignore_lame_headers from the medialib */
			data->frames_to_skip = 1;
			data->samples_to_skip = lame->start_delay;
			data->samples_to_play = ((guint64) xmms_xing_get_frames (data->xing) * 1152ULL) -
			                        lame->start_delay - lame->end_padding;
			XMMS_DBG ("Samples to skip in the beginning: %d, total: %" G_GINT64_FORMAT,
			          data->samples_to_skip, data->samples_to_play);
			/*
			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM;
			xmms_xform_metadata_set_int (xform, metakey, lame->audiophile_gain);

			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK;
			xmms_xform_metadata_set_int (xform, metakey, lame->peak_amplitude);

			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK;
			xmms_xform_metadata_set_int (xform, metakey, lame->radio_gain);
			*/
		}

	} else {
		gint filesize;

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE;
		xmms_xform_metadata_set_int (xform, metakey, frame.header.bitrate);

		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		if (!xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;

			if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
				gint32 val;

				val = (gint32) (filesize * (gdouble) 8000.0 / frame.header.bitrate);

				metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
				xmms_xform_metadata_set_int (xform, metakey, val);
			}
		}
	}

	/* seeking needs bitrate */
	data->bitrate = frame.header.bitrate;

	if (xmms_id3v1_get_tags (xform) < 0) {
		mad_stream_finish (&data->stream);
		mad_frame_finish (&data->frame);
		mad_synth_finish (&data->synth);
		if (data->xing) {
			xmms_xing_free (data->xing);
		}
		return FALSE;
	}

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->samplerate,
	                             XMMS_STREAM_TYPE_END);

	mad_frame_finish (&frame);
	mad_stream_finish (&stream);

	return TRUE;
}

static inline gint16
scale_linear (mad_fixed_t v)
{
	/* make sure rounding is correct during clipping */
	v += (1L << (MAD_F_FRACBITS - 16));
	if (v >= MAD_F_ONE) {
		v = MAD_F_ONE - 1;
	} else if (v < -MAD_F_ONE) {
		v = MAD_F_ONE;
	}

	return v >> (MAD_F_FRACBITS - 15);
}

static gint
xmms_mad_read (xmms_xform_t *xform, gpointer buf, gint len, xmms_error_t *err)
{
	xmms_mad_data_t *data;
	xmms_samples16_t *out = (xmms_samples16_t *)buf;
	gint ret;
	gint j;
	gint read = 0;

	data = xmms_xform_private_data_get (xform);

	j = 0;

	while (read < len) {

		/* use already synthetized frame first */
		if (data->synthpos < data->synth.pcm.length) {
			out[j++] = scale_linear (data->synth.pcm.samples[0][data->synthpos]);
			if (data->channels == 2) {
				out[j++] = scale_linear (data->synth.pcm.samples[1][data->synthpos]);
				read += 2 * xmms_sample_size_get (XMMS_SAMPLE_FORMAT_S16);
			} else {
				read += xmms_sample_size_get (XMMS_SAMPLE_FORMAT_S16);
			}
			data->synthpos++;
			continue;
		}

		/* then try to decode another frame */
		if (mad_frame_decode (&data->frame, &data->stream) != -1) {

			/* mad_synthpop_frame - go Depeche! */
			mad_synth_frame (&data->synth, &data->frame);

			if (data->frames_to_skip) {
				data->frames_to_skip--;
				data->synthpos = 0x7fffffff;
			} else if (data->samples_to_skip) {
				if (data->samples_to_skip > data->synth.pcm.length) {
					data->synthpos = 0x7fffffff;
					data->samples_to_skip -= data->synth.pcm.length;
				} else {
					data->synthpos = data->samples_to_skip;
					data->samples_to_skip = 0;
				}
			} else {
				if (data->samples_to_play == 0) {
					return read;
				} else if (data->samples_to_play > 0) {
					if (data->synth.pcm.length > data->samples_to_play) {
						data->synth.pcm.length = data->samples_to_play;
					}
					data->samples_to_play -= data->synth.pcm.length;
				}
				data->synthpos = 0;
			}
			continue;
		}


		/* if there is no frame to decode stream more data */
		if (data->stream.next_frame) {
			guchar *buffer = data->buffer;
			const guchar *nf = data->stream.next_frame;
			memmove (data->buffer, data->stream.next_frame,
			         data->buffer_length = (&buffer[data->buffer_length] - nf));
		}

		ret = xmms_xform_read (xform,
		                       (gchar *)data->buffer + data->buffer_length,
		                       4096 - data->buffer_length,
		                       err);

		if (ret <= 0) {
			return ret;
		}

		data->buffer_length += ret;
		mad_stream_buffer (&data->stream, data->buffer, data->buffer_length);
	}

	return read;
}
