/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */




/** @file
 * MPEG Layer 1/2/3 decoder plugin.
 *
 * Supports Xing VBR and id3v1/v2
 *
 * This code basicly sucks at some places. But hey, the
 * standard is fucked. Please convert to Ogg ;-)
 */


#include "xmms/xmms_defs.h"
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"
#include "id3v1.h"
#include "xing.h"
#include <mad.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

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

	xmms_xing_t *xing;
} xmms_mad_data_t;


/*
 * Function prototypes
 */

static gboolean xmms_mad_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_mad_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static void xmms_mad_destroy (xmms_xform_t *decoder);
static gboolean xmms_mad_init (xmms_xform_t *decoder);
static gint64 xmms_mad_seek(xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN("mad",
                  "MAD decoder",
                  XMMS_VERSION,
                  "MPEG Layer 1/2/3 decoder",
                  xmms_mad_plugin_setup);

static gboolean
xmms_mad_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT(methods);
	methods.init = xmms_mad_init;
	methods.destroy = xmms_mad_destroy;
	methods.read = xmms_mad_read;
	methods.seek = xmms_mad_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	  xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	  xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	  xmms_plugin_info_add (plugin, "License", "GPL");
	*/

	/* xmms_xform_indata_constraint_add */
	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/mpeg",
	                              NULL);

	xmms_magic_add ("mpeg header", "audio/mpeg",
	                "0 beshort&0xfff6 0xfff6",
	                "0 beshort&0xfff6 0xfff4",
	                "0 beshort&0xffe6 0xffe2",
	                NULL);

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

	g_free (data);

}

static gint64
xmms_mad_seek(xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_mad_data_t *data;
	guint bytes;
	gint64 res;

	g_return_val_if_fail (whence == XMMS_XFORM_SEEK_SET, -1);
	g_return_val_if_fail (xform, -1);

	data = xmms_xform_private_data_get (xform);

	if (data->xing) {
		guint i;
		guint x_samples;

		x_samples = xmms_xing_get_frames (data->xing) * 1152;

		i = (guint) (100.0 * (gdouble) samples) / (gdouble) x_samples;

		bytes = xmms_xing_get_toc (data->xing, i) * xmms_xing_get_bytes (data->xing) / 256;
	} else {
		bytes = (guint)(((gdouble)samples) * data->bitrate / data->samplerate) / 8;
	}

	XMMS_DBG ("Try seek %lld samples -> %d bytes", samples, bytes);

	res = xmms_xform_seek (xform, bytes, XMMS_XFORM_SEEK_SET, err);
	if (res == -1) {
		return -1;
	}
	return samples;
}

static void
xmms_mad_get_id3v1 (xmms_xform_t *xform)
{
	xmms_error_t err;
	gint64 res;
	guchar buf[128];

	xmms_error_reset (&err);

	res = xmms_xform_seek (xform, -128, XMMS_XFORM_SEEK_END, &err);

	if (res == -1) {
		XMMS_DBG ("Couldn't seek - not getting id3v1 tag");
		return;
	}

	if (xmms_xform_read (xform, buf, 128, &err) == 128) {
		xmms_mad_id3v1_parse (xform, buf);
	} else {
		XMMS_DBG ("Read of 128 bytes failed?!");
		xmms_error_reset (&err);
	}

	res = xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_SET, &err);
	if (res == -1) {
		XMMS_DBG ("Couldn't seek after getting id3 tag?!? very bad");
	}
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
			return FALSE;
		}
	}

	data->samplerate = frame.header.samplerate;
	data->channels = frame.header.mode == MAD_MODE_SINGLE_CHANNEL ? 1 : 2;

	if (frame.header.flags & MAD_FLAG_PROTECTION) {
		XMMS_DBG ("Frame has protection enabled");
		if (stream.anc_ptr.byte > stream.buffer + 2) {
			stream.anc_ptr.byte = stream.anc_ptr.byte - 2;
		}
	}

	data->xing = xmms_xing_parse (stream.anc_ptr);
	if (data->xing) {
		XMMS_DBG ("File with Xing header!");

		xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_IS_VBR, 1);

		if (xmms_xing_has_flag (data->xing, XMMS_XING_FRAMES)) {
			guint duration;
			mad_timer_t timer;
			
			timer = frame.header.duration;
			mad_timer_multiply (&timer, xmms_xing_get_frames (data->xing));
			duration = mad_timer_count (timer, MAD_UNITS_MILLISECONDS);

			XMMS_DBG ("XING duration %d", duration);

			xmms_xform_metadata_set_int (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			                             duration);

			if (xmms_xing_has_flag (data->xing, XMMS_XING_BYTES) && duration) {
				guint tmp;

				tmp = xmms_xing_get_bytes (data->xing) * ((guint64)8000) / duration;
				XMMS_DBG ("XING bitrate %d", tmp);
				xmms_xform_metadata_set_int (xform,
				                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
				                             tmp);
			}
		}

	} else {
		gint filesize;

		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
		                             frame.header.bitrate);


		filesize = xmms_xform_metadata_get_int (xform, XMMS_XFORM_DATA_SIZE);
		if (filesize == -1) {
			xmms_xform_metadata_set_int (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			                             -1);
		} else {
			xmms_xform_metadata_set_int (xform,
			                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			                             (gint) (filesize*(gdouble)8000.0/frame.header.bitrate));
		}
	}

	/* seeking needs bitrate */
	data->bitrate = frame.header.bitrate;

	xmms_mad_get_id3v1 (xform);

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
			data->synthpos = 0;
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
