/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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
#include "xmms/xmms_decoderplugin.h"
#include "xmms/xmms_log.h"
#include "id3.h"
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
	
	xmms_xing_t *xing;
} xmms_mad_data_t;


/*
 * Function prototypes
 */

static gboolean xmms_mad_new (xmms_decoder_t *decoder);
static gboolean xmms_mad_decode_block (xmms_decoder_t *decoder);
static void xmms_mad_get_media_info (xmms_decoder_t *decoder);
static void xmms_mad_destroy (xmms_decoder_t *decoder);
static gboolean xmms_mad_init (xmms_decoder_t *decoder, gint mode);
static gboolean xmms_mad_seek (xmms_decoder_t *decoder, guint samples);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, 
	                          XMMS_DECODER_PLUGIN_API_VERSION,
	                          "mad",
	                          "MAD decoder " XMMS_VERSION,
	                          "MPEG Layer 1/2/3 decoder");
	
	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	xmms_plugin_info_add (plugin, "License", "GPL");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_mad_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_mad_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_mad_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_mad_get_media_info);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_mad_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_mad_seek);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	xmms_plugin_magic_add (plugin, "id3 header", "audio/mpeg",
	                       "0 string ID3", ">3 byte <0xff",
	                       ">>4 byte <0xff", NULL);
	xmms_plugin_magic_add (plugin, "mpeg header", "audio/mpeg",
	                       "0 beshort&0xfff6 0xfff6",
	                       "0 beshort&0xfff6 0xfff4",
	                       "0 beshort&0xffe6 0xffe2",
	                       NULL);

	return plugin;
}

static void
xmms_mad_destroy (xmms_decoder_t *decoder)
{
	xmms_mad_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	mad_stream_finish (&data->stream);
	mad_frame_finish (&data->frame);
	mad_synth_finish (&data->synth);

	g_free (data);

}

static gboolean
xmms_mad_seek (xmms_decoder_t *decoder, guint samples)
{
	xmms_mad_data_t *data;
	guint bytes;
	
	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);

	if (data->xing) {
		guint i;
		guint x_samples;

		x_samples = xmms_xing_get_frames (data->xing) * 1152;

		i = (guint) (100.0 * (gdouble) samples) / (gdouble) x_samples;

		bytes = xmms_xing_get_toc (data->xing, i) * xmms_xing_get_bytes (data->xing) / 256;
	} else {
		bytes = (guint)(((gdouble)samples) * data->bitrate / data->samplerate) / 8;
	}

	XMMS_DBG ("Try seek %d bytes", bytes);

	if (bytes > data->fsize) {
		xmms_log_error ("To big value %llu is filesize", data->fsize);
		return FALSE;
	}

	xmms_transport_seek (xmms_decoder_transport_get (decoder), bytes, 
			XMMS_TRANSPORT_SEEK_SET);

	return TRUE;
}

/** This function will calculate the duration in seconds.
  *
  * This is very easy, until someone thougth that VBR was
  * a good thing. That is a quite fucked up standard.
  *
  * We read the XING header and parse it accordingly to
  * xing.c, then calculate the duration of the next frame
  * and multiply it with the number of frames in the XING
  * header.
  *
  * Better than to stream the whole file I guess.
  */

static void
xmms_mad_calc_duration (xmms_medialib_session_t *session,
                        xmms_decoder_t *decoder,
                        guchar *buf, gint len,
                        gint filesize,
                        xmms_medialib_entry_t entry)
{
	struct mad_frame frame;
	struct mad_stream stream;
	xmms_mad_data_t *data;
	guint bitrate=0;

	data = xmms_decoder_private_data_get (decoder);

	mad_stream_init (&stream);
	mad_frame_init (&frame);

	mad_stream_buffer (&stream, buf, len);

	while (mad_frame_decode (&frame, &stream) == -1) {
		if (!MAD_RECOVERABLE (stream.error)) {
			XMMS_DBG ("couldn't decode %02x %02x %02x %02x",buf[0],buf[1],buf[2],buf[3]);
			return;
		}
	}

	data->samplerate = frame.header.samplerate;
	data->channels = MAD_NCHANNELS (&frame.header);

	if (filesize == -1) {
		xmms_medialib_entry_property_set_int (session,
		                                      entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
		                                      -1);

		/* frame.header.bitrate might be wrong, but we cannot do it any
		 * better for streams
		 */
		xmms_medialib_entry_property_set_int (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
		                                      frame.header.bitrate);
		return;
	}

	data->fsize = filesize;

	data->xing = xmms_xing_parse (stream.anc_ptr);
	if (data->xing) {
		/* @todo Hmm? This is SO strange. */
		while (42) {
			if (mad_frame_decode (&frame, &stream) == -1) {
				if (MAD_RECOVERABLE (stream.error))
					continue;
				break;
			}
		}

		xmms_medialib_entry_property_set_int (session,
		                                      entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_IS_VBR,
		                                      1);

		if (xmms_xing_has_flag (data->xing, XMMS_XING_FRAMES)) {
			guint duration;
			mad_timer_t timer;

			timer = frame.header.duration;
			mad_timer_multiply (&timer, xmms_xing_get_frames (data->xing));
			duration = mad_timer_count (timer, MAD_UNITS_MILLISECONDS);

			XMMS_DBG ("XING duration %d", duration);

			xmms_medialib_entry_property_set_int (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
			                                      duration);

			if (xmms_xing_has_flag (data->xing, XMMS_XING_BYTES)) {
				guint tmp;

				tmp = xmms_xing_get_bytes (data->xing) * 8 / duration * 1000;
				XMMS_DBG ("XING bitrate %d", tmp);
				xmms_medialib_entry_property_set_int (session,
				                                      entry,
				                                      XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
				                                      tmp);
			}
		}

		return;
	}

	data->bitrate = bitrate = frame.header.bitrate;

	mad_frame_finish (&frame);
	mad_stream_finish (&stream);

	if (filesize == -1) {
		xmms_medialib_entry_property_set_int (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
		                                      -1);
	} else {
		xmms_medialib_entry_property_set_int (session, entry,
		                                      XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
		                                      (gint) (filesize*(gdouble)8000.0/bitrate));
	}

	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
	                                      bitrate);

}

static void
xmms_mad_get_media_info (xmms_decoder_t *decoder)
{
	xmms_transport_t *transport;
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	xmms_mad_data_t *data;
	xmms_id3v2_header_t head;
	xmms_error_t error;
	guchar buf[8192];
	gboolean id3handled = FALSE;
	gint ret;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);

	transport = xmms_decoder_transport_get (decoder);
	g_return_if_fail (transport);

	entry = xmms_decoder_medialib_entry_get (decoder);

	ret = xmms_transport_read (transport, (gchar *)buf, 8192, &error);
	if (ret <= 0) {
		return;
	}

	session = xmms_medialib_begin_write ();

	if (xmms_transport_islocal (transport) && 
			ret >= 10 && 
			xmms_mad_id3v2_header (buf, &head)) {
		guchar *id3v2buf;
		gint pos;

		/** @todo sanitycheck head.len */

		XMMS_DBG ("id3v2 len = %d", head.len);

		id3v2buf = g_malloc (head.len);
		
		memcpy (id3v2buf, buf+10, MIN (ret-10,head.len));
		
		if (ret-10 < head.len) { /* need more data */
			pos = MIN (ret-10,head.len);
			
			while (pos < head.len) {
				ret = xmms_transport_read (transport,
							   (gchar *)id3v2buf + pos,
							   MIN(4096,head.len - pos), &error);
				if (ret <= 0) {
					xmms_log_error ("error reading data for id3v2-tag");
					xmms_medialib_end (session);
					return;
				}
				pos += ret;
			}
			ret = xmms_transport_read (transport,(gchar *) buf, 8192, &error);
		} else {
			/* just make sure buf is full */
			memmove (buf, buf + head.len + 10, 8192 - (head.len+10));
			ret += xmms_transport_read (transport, (gchar *)buf + 8192 - (head.len+10), head.len + 10, &error) - head.len - 10;
		}
		
		id3handled = xmms_mad_id3v2_parse (session, id3v2buf, &head, entry);
		g_free (id3v2buf);
	}
	
	xmms_mad_calc_duration (session, decoder, buf, ret, xmms_transport_size (transport), entry);

	if (xmms_transport_islocal (transport) && !id3handled) {
		xmms_transport_seek (transport, -128, XMMS_TRANSPORT_SEEK_END);
		ret = xmms_transport_read (transport, (gchar *)buf, 128, &error);
		if (ret == 128) {
			xmms_mad_id3_parse (session, buf, entry);
		}
	}

	xmms_medialib_entry_property_set_int (session, entry, 
										  XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE,
										  data->samplerate);


	xmms_transport_seek (transport, 0, XMMS_TRANSPORT_SEEK_SET);

	xmms_medialib_end (session);
	xmms_medialib_entry_send_update (entry);

	return;
}

static gboolean
xmms_mad_new (xmms_decoder_t *decoder)
{
	xmms_mad_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = g_new0 (xmms_mad_data_t, 1);

	mad_stream_init (&data->stream);
	mad_frame_init (&data->frame);
	mad_synth_init (&data->synth);

	xmms_decoder_private_data_set (decoder, data);
	
	return TRUE;
}

static gboolean
xmms_mad_init (xmms_decoder_t *decoder, gint mode)
{
	xmms_transport_t *transport;
	xmms_mad_data_t *data;

	g_return_val_if_fail (decoder, FALSE);
	
	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);
	
	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (decoder, FALSE);
	
	data->buffer_length = 0;

	if (mode & XMMS_DECODER_INIT_DECODING) {
		xmms_mad_get_media_info (decoder);
		
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_S16, data->channels, data->samplerate);
		/* we don't have to care about the return value
		   other than NULL,
		   as there is only one format (to rule them all) */
		if (xmms_decoder_format_finish (decoder) == NULL) {
			return FALSE;
		}
	}

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

static gboolean
xmms_mad_decode_block (xmms_decoder_t *decoder)
{
	xmms_mad_data_t *data;
	xmms_transport_t *transport;
	xmms_error_t error;
	gint16 out[1152 * 2];
	mad_fixed_t *ch1, *ch2;
	gint ret;

	data = xmms_decoder_private_data_get (decoder);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	if (data->stream.next_frame) {
		guchar *buffer = data->buffer;
		const guchar *nf = data->stream.next_frame;
		memmove (data->buffer, data->stream.next_frame,
		         data->buffer_length = (&buffer[data->buffer_length] - nf));
	} 
	
	ret = xmms_transport_read (transport, 
				   (gchar *)data->buffer + data->buffer_length,
				   4096 - data->buffer_length,
				   &error);
	
	if (ret <= 0) {
		XMMS_DBG ("EOF");
		return FALSE;
	}

	data->buffer_length += ret;
	mad_stream_buffer (&data->stream, data->buffer, data->buffer_length);

	for (;;) {
		gint i = 0;
		gint j = 0;

		if (mad_frame_decode (&data->frame, &data->stream) == -1) {
			break;
		}

			
		/* mad_synthpop_frame - go Depeche! */
		mad_synth_frame (&data->synth, &data->frame);
		
		ch1 = data->synth.pcm.samples[0];
		ch2 = data->synth.pcm.samples[1];

		for (i = 0; i < data->synth.pcm.length; i++) {
			out[j++] = scale_linear (*(ch1++));
			if (data->channels == 2)
				out[j++] = scale_linear (*(ch2++));
		}

		ret = j * xmms_sample_size_get (XMMS_SAMPLE_FORMAT_S16);

		xmms_decoder_write (decoder, (gchar *)out, ret);

	}
	
	return TRUE;
}
