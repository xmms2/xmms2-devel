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
 * standard is fucked. Please convert to OGG ;-)
 */


#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/core.h"
#include "xmms/playlist.h"
#include "xmms/transport.h"
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

	xmms_playlist_entry_t *entry;

	gchar buffer[4096];
	guint buffer_length;
	guint bitrate;
	guint fsize;
	
	xmms_xing_t *xing;
} xmms_mad_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_mad_can_handle (const gchar *mimetype);
static gboolean xmms_mad_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_mad_decode_block (xmms_decoder_t *decoder);
static void xmms_mad_get_media_info (xmms_decoder_t *decoder);
static void xmms_mad_destroy (xmms_decoder_t *decoder);
static gboolean xmms_mad_init (xmms_decoder_t *decoder);
static gboolean xmms_mad_seek (xmms_decoder_t *decoder, guint samples);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "mad",
			"MAD decoder " XMMS_VERSION,
			"MPEG Layer 1/2/3 decoder");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	xmms_plugin_info_add (plugin, "License", "GPL");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_mad_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_mad_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_mad_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_mad_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_mad_get_media_info);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_mad_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_mad_seek);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

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

	XMMS_DBG ("seek samples %d", samples);

	if (data->xing) {
		guint i;
		guint x_samples;

		x_samples = xmms_xing_get_frames (data->xing) * 1152;

		i = (guint) (100.0 * (gdouble) samples) / (gdouble) x_samples;
		XMMS_DBG ("i = %d x_samples = %d", i, x_samples);

		bytes = xmms_xing_get_toc (data->xing, i) * xmms_xing_get_bytes (data->xing) / 256;
	} else {
		bytes = (guint)(((gdouble)samples) * data->bitrate / xmms_decoder_samplerate_get (decoder)) / 8;
	}

	XMMS_DBG ("Try seek %d bytes", bytes);

	if (bytes > data->fsize) {
		XMMS_DBG ("To big value %d is filesize", data->fsize);
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
xmms_mad_calc_duration (xmms_decoder_t *decoder, gchar *buf, gint len, guint filesize, xmms_playlist_entry_t *entry)
{
	struct mad_frame frame;
	struct mad_stream stream;
	xmms_mad_data_t *data;
	xmms_xing_t *xing;
	guint fsize=0;
	guint bitrate=0;
	gchar *tmp;

	data = xmms_decoder_private_data_get (decoder);

	XMMS_DBG ("Buffer is %d bytes", len);

	mad_stream_init (&stream);
	mad_frame_init (&frame);

	mad_stream_buffer (&stream, buf, len);

	while (mad_frame_decode (&frame, &stream) == -1) {
		if (!MAD_RECOVERABLE (stream.error)) {
			
			XMMS_DBG ("couldn't decode %02x %02x %02x %02x",buf[0],buf[1],buf[2],buf[3]);
			return;
		}
	}

	xmms_decoder_samplerate_set (decoder,
		     frame.header.samplerate);
	
	fsize = filesize * 8;
	data->fsize = filesize;

	if ((xing = xmms_xing_parse (stream.anc_ptr))) {
		
		/* @todo Hmm? This is SO strange. */
		while (42) {
			if (mad_frame_decode (&frame, &stream) == -1) {
				if (MAD_RECOVERABLE (stream.error))
					continue;
				break;
			}
		}

		data->xing = xing;

		if (xmms_xing_has_flag (xing, XMMS_XING_FRAMES)) {
			guint duration;
			mad_timer_t timer;
			gchar *tmp;

			timer = frame.header.duration;
			mad_timer_multiply (&timer, xmms_xing_get_frames (xing));
			duration = mad_timer_count (timer, MAD_UNITS_MILLISECONDS);

			XMMS_DBG ("XING duration %d", duration);
			tmp = g_strdup_printf ("%d", duration);

			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, tmp);
			g_free (tmp);
		}

		/** @todo fix avg. bitrate in xing */
/*		if (xmms_xing_has_flag (xing, XMMS_XING_BYTES)) {
			gchar *tmp;

			tmp = g_strdup_printf ("%u", (gint)((xmms_xing_get_bytes (xing) * 8 / fsize);
			XMMS_DBG ("XING bitrate %d", tmp);
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE, tmp);
			g_free (tmp);
		}*/

		return;
	}

	data->bitrate = bitrate = frame.header.bitrate;

	mad_frame_finish (&frame);
	mad_stream_finish (&stream);

	if (!fsize) {
		xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, "-1");
	} else {
		tmp = g_strdup_printf ("%d", (gint) (filesize*(gdouble)8000.0/bitrate));
		xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION, tmp);
		XMMS_DBG ("duration = %s", tmp);
		g_free (tmp);
	}
		
	tmp = g_strdup_printf ("%d", bitrate / 1000);
	xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE, tmp);
	g_free (tmp);

}

static void
xmms_mad_get_media_info (xmms_decoder_t *decoder)
{
	xmms_transport_t *transport;
	xmms_playlist_entry_t *entry;
	xmms_mad_data_t *data;
	xmms_id3v2_header_t head;
	gchar buf[8192];
	gboolean id3handled = FALSE;
	gint ret;

	g_return_if_fail (decoder);

	data = xmms_decoder_private_data_get (decoder);

	transport = xmms_decoder_transport_get (decoder);
	g_return_if_fail (transport);

	entry = xmms_playlist_entry_new (NULL);

	ret = xmms_transport_read (transport, buf, 8192);
	if (ret <= 0) {
		xmms_object_unref (entry);
		return;
	}

	if (xmms_transport_islocal (transport) && 
			ret >= 10 && 
			xmms_mad_id3v2_header (buf, &head)) {
		gchar *id3v2buf;
		gint pos;

		/** @todo sanitycheck head.len */

		XMMS_DBG ("id3v2 len = %d", head.len);

		id3v2buf = g_malloc (head.len);
		
		memcpy (id3v2buf, buf+10, MIN (ret-10,head.len));
		
		if (ret-10 < head.len) { /* need more data */
			pos = MIN (ret-10,head.len);
			
			while (pos < head.len) {
				ret = xmms_transport_read (transport,
							   id3v2buf + pos,
							   MIN(4096,head.len - pos));
				if (ret <= 0) {
					XMMS_DBG ("error reading data for id3v2-tag");
					xmms_object_unref (entry);
					return;
				}
				pos += ret;
			}
			ret = xmms_transport_read (transport, buf, 8192);
		} else {
			/* just make sure buf is full */
			memmove (buf, buf + head.len + 10, 8192 - (head.len+10));
			ret += xmms_transport_read (transport, buf + 8192 - (head.len+10), head.len + 10) - head.len - 10;
		}
		
		id3handled = xmms_mad_id3v2_parse (id3v2buf, &head, entry);
	}
	
	xmms_mad_calc_duration (decoder, buf, ret, xmms_transport_size (transport), entry);

	if (xmms_transport_islocal (transport) && !id3handled) {
		XMMS_DBG ("Seeking to last 128 bytes");
		xmms_transport_seek (transport, -128, XMMS_TRANSPORT_SEEK_END);
		ret = xmms_transport_read (transport, buf, 128);
		XMMS_DBG ("Got %d bytes, Seeking to last first bytes", ret);
		xmms_transport_seek (transport, 0, XMMS_TRANSPORT_SEEK_SET);
		if (ret == 128) {
			xmms_mad_id3_parse (buf, entry);
		}
	}

	if (data) {
		data->entry = entry;

		xmms_decoder_entry_mediainfo_set (decoder, entry);
	}

	xmms_object_unref (entry);

	return;
}

static gboolean
xmms_mad_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);
	
	if ((g_strcasecmp (mimetype, "audio/mpeg") == 0))
		return TRUE;

	return FALSE;

}

static gboolean
xmms_mad_new (xmms_decoder_t *decoder, const gchar *mimetype)
{
	xmms_mad_data_t *data;

	g_return_val_if_fail (decoder, FALSE);
	g_return_val_if_fail (mimetype, FALSE);

	data = g_new0 (xmms_mad_data_t, 1);

	mad_stream_init (&data->stream);
	mad_frame_init (&data->frame);
	mad_synth_init (&data->synth);

	data->entry = NULL;

	xmms_decoder_private_data_set (decoder, data);
	
	return TRUE;
}

static gboolean
xmms_mad_init (xmms_decoder_t *decoder)
{
	xmms_transport_t *transport;
	xmms_mad_data_t *data;

	g_return_val_if_fail (decoder, FALSE);
	
	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);
	
	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (decoder, FALSE);
	
	data->buffer_length = 0;
	xmms_mad_get_media_info (decoder);

	return TRUE;
}

gint
clipping (mad_fixed_t v)
{
	if (v >= 1 << MAD_F_FRACBITS)
		v = (1 << MAD_F_FRACBITS) - 1;

	if (v <= -(1 << MAD_F_FRACBITS))
		v = -((1 << MAD_F_FRACBITS) - 1);

	return v >> (MAD_F_FRACBITS - 15);
}

static gboolean
xmms_mad_decode_block (xmms_decoder_t *decoder)
{
	xmms_mad_data_t *data;
	xmms_transport_t *transport;
	gint16 out[1152 * 2];
	mad_fixed_t *ch1, *ch2;
	gint ret;

	data = xmms_decoder_private_data_get (decoder);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	if (data->stream.next_frame) {
		gchar *buffer = data->buffer;
		const gchar *nf = data->stream.next_frame;
		memmove (data->buffer, data->stream.next_frame,
				 data->buffer_length = (&buffer[data->buffer_length] - nf));
	} 
	
	ret = xmms_transport_read (transport, 
				   data->buffer + data->buffer_length,
				   4096 - data->buffer_length);
	if (ret <= 0) {
		XMMS_DBG ("EOF");
		return FALSE;
	}

	data->buffer_length += ret;
	mad_stream_buffer (&data->stream, data->buffer, data->buffer_length);

	for (;;) {
		gint i = 0;

		if (mad_frame_decode (&data->frame, &data->stream) == -1) {
			break;
		}

			
		/* mad_synthpop_frame - go Depeche! */
		mad_synth_frame (&data->synth, &data->frame);
		
		ch1 = data->synth.pcm.samples[0];
		ch2 = data->synth.pcm.samples[1];

		for (i = 0; i < data->synth.pcm.length; i++) {
			gint l, r;
			
			l = clipping (*(ch1++));
			if (data->synth.pcm.channels > 1)
				r = clipping (*(ch2++));
			else
				r = l;
			
			out[i*2+0] = l;
			out[i*2+1] = r;
		}

		ret = i*4;

		xmms_decoder_write (decoder, (gchar *)out, ret);

	}
	
	return TRUE;
}

