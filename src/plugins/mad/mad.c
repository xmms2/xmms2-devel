#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/core.h"
#include "xmms/output.h"
#include "xmms/playlist.h"
#include "xmms/transport.h"
#include "mad_misc.h"
#include "id3.h"
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
} xmms_mad_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_mad_can_handle (const gchar *mimetype);
static gboolean xmms_mad_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_mad_decode_block (xmms_decoder_t *decoder);
static xmms_playlist_entry_t *xmms_mad_get_media_info (xmms_decoder_t *decoder);
static void xmms_mad_destroy (xmms_decoder_t *decoder);
static gboolean xmms_mad_init (xmms_decoder_t *decoder);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "mad",
			"MAD decoder " VERSION,
			"MPEG Layer 1/2/3 decoder");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_mad_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, xmms_mad_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_mad_decode_block);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_DESTROY, xmms_mad_destroy);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_GET_MEDIAINFO, xmms_mad_get_media_info);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_mad_init);


	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_FAST_FWD);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_REWIND);

	return plugin;
}

static void
xmms_mad_destroy (xmms_decoder_t *decoder)
{
	xmms_mad_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_if_fail (data);

	mad_stream_finish (&data->stream);
	mad_frame_finish (&data->frame);
	mad_synth_finish (&data->synth);

	g_free (data);

}


static void
xmms_mad_calc_duration (gchar *buf, gint len, guint filesize, xmms_playlist_entry_t *entry)
{
	struct mad_header header;
	struct mad_stream stream;
	guint fsize=0;
	guint bitrate=0;
	gchar *tmp;

	mad_stream_init (&stream);

	mad_stream_buffer (&stream, buf, len);
		
	if (mad_header_decode (&header, &stream) == -1) {
		XMMS_DBG ("couldn't decode %02x %02x %02x %02x",buf[0],buf[1],buf[2],buf[3]);
		return;
	}


	fsize = filesize * 8;
	bitrate = header.bitrate;

	mad_header_finish (&header);
	mad_stream_finish (&stream);

	if (!fsize) {
		xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_DURATION, "-1");
	} else {
		tmp = g_strdup_printf ("%d", fsize / bitrate);
		xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_DURATION, tmp);
		XMMS_DBG ("duration = %s", tmp);
		g_free (tmp);
	}
		
	tmp = g_strdup_printf ("%d", bitrate / 1000);
	xmms_playlist_entry_set_prop (entry, XMMS_ENTRY_PROPERTY_BITRATE, tmp);
	g_free (tmp);

}

static xmms_playlist_entry_t *
xmms_mad_get_media_info (xmms_decoder_t *decoder)
{
	xmms_transport_t *transport;
	xmms_playlist_entry_t *entry;
	xmms_mad_data_t *data;
	xmms_id3v2_header_t head;
	gchar buf[4096];
	gboolean id3handled = FALSE;
	gint ret;

	g_return_val_if_fail (decoder, NULL);

	data = xmms_decoder_plugin_data_get (decoder);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, NULL);

	entry = xmms_playlist_entry_new (NULL);

	ret = xmms_transport_read (transport, buf, 4096);
	if (ret <= 0) {
		return entry;
	}

	if (ret >= 10 && xmms_mad_id3v2_header (buf, &head)) {
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
							   head.len - pos);
				if (ret <= 0) {
					XMMS_DBG ("error reading data for id3v2-tag");
					return entry;
				}
				pos += ret;
			}
			ret = xmms_transport_read (transport, buf, 4096);
		} else {
			/* just make sure buf is full */
			memmove (buf, buf + head.len + 10, 4096 - (head.len+10));
			ret = xmms_transport_read (transport, buf + 4096 - (head.len+10), head.len + 10);
		}
		
		id3handled = xmms_mad_id3v2_parse (id3v2buf, &head, entry);
	}
	
	xmms_mad_calc_duration (buf, ret, xmms_transport_size (transport), entry);

	if (!id3handled) {
		XMMS_DBG ("Seeking to last 128 bytes");
		xmms_transport_seek (transport, -128, XMMS_TRANSPORT_SEEK_END);
		ret = xmms_transport_read (transport, buf, 128);
		XMMS_DBG ("Seeking to last first bytes");
		xmms_transport_seek (transport, 0, XMMS_TRANSPORT_SEEK_SET);
		if (ret == 128) {
			xmms_mad_id3_parse (buf, entry);
		}
	}

	if (data) {
		data->entry = entry;

		xmms_core_set_mediainfo (entry);
	}

	return entry;
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

	xmms_decoder_plugin_data_set (decoder, data);
	
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
	
	data = xmms_decoder_plugin_data_get (decoder);
	g_return_val_if_fail (decoder, FALSE);
	
	data->buffer_length = 0;
	xmms_mad_get_media_info (decoder);

	return TRUE;
}

static gboolean
xmms_mad_decode_block (xmms_decoder_t *decoder)
{
	xmms_mad_data_t *data;
	xmms_transport_t *transport;
	xmms_output_t *output;
	gchar out[1152 * 4];
	mad_fixed_t *ch1, *ch2;
	mad_fixed_t clipping = 0;
	gulong clipped;
	gint ret;

	data = xmms_decoder_plugin_data_get (decoder);

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);

	if (data->stream.next_frame) {
		gchar *buffer = data->buffer, *nf = data->stream.next_frame;
		memmove (data->buffer, data->stream.next_frame,
				 data->buffer_length = (&buffer[data->buffer_length] - nf));
	} 
	
	output = xmms_decoder_output_get (decoder);
	g_return_val_if_fail (output, FALSE);

	ret = xmms_transport_read (transport, data->buffer + data->buffer_length,
							   4096 - data->buffer_length);
	if (ret <= 0) {
		XMMS_DBG ("EOF");
		return FALSE;
	}

	data->buffer_length += ret;
	mad_stream_buffer (&data->stream, data->buffer, data->buffer_length);
		
	for (;;) {

		if (mad_frame_decode (&data->frame, &data->stream) == -1) {
			break;
		}

		mad_synth_frame (&data->synth, &data->frame);
		
		ch1 = data->synth.pcm.samples[0];
		ch2 = data->synth.pcm.samples[1];
		
		ret = pack_pcm (out, data->synth.pcm.length, ch1, ch2, 16, &clipped, &clipping);
		xmms_output_write (output, out, sizeof(out));
		
	}
	
	return TRUE;
}

