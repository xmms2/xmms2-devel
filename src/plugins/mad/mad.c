#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"
#include "mad_misc.h"
#include <mad.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * Type definitions
 */


struct id3v1tag_t {
        char tag[3]; /* always "TAG": defines ID3v1 tag 128 bytes before EOF */
        char title[30];
        char artist[30];
        char album[30];
        char year[4];
        union {
                struct {
                        char comment[30];
                } v1_0;
                struct {
                        char comment[28];
                        char __zero;
                        unsigned char track_number;
                } v1_1;
        } u;
        unsigned char genre;
};


typedef struct xmms_mad_data_St {
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;

	gchar buffer[4096];
	guint buffer_length;
} xmms_mad_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_mad_can_handle (const gchar *mimetype);
static gboolean xmms_mad_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_mad_decode_block (xmms_decoder_t *decoder);
static void xmms_mad_get_media_info (xmms_decoder_t *decoder);
static void xmms_mad_destroy (xmms_decoder_t *decoder);

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

	xmms_plugin_method_add (plugin, "can_handle", xmms_mad_can_handle);
	xmms_plugin_method_add (plugin, "new", xmms_mad_new);
	xmms_plugin_method_add (plugin, "decode_block", xmms_mad_decode_block);
	xmms_plugin_method_add (plugin, "get_media_info", xmms_mad_get_media_info);
	xmms_plugin_method_add (plugin, "destroy", xmms_mad_destroy);

	return plugin;
}

static void
xmms_mad_destroy (xmms_decoder_t *decoder)
{
	xmms_mad_data_t *data;

	g_return_if_fail (decoder);

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_if_fail (data);

	g_free (data);

}

static void
remove_trail_space (gchar *str)
{

	int len = strlen (str);
	int i;

	for (i=(len-1); i > 0; i--) {
		if (!isspace (str[i])) {
			str[i+1] = '\0';
			return;
		}
	}

	str[i] = '\0';
}

static void
xmms_mad_get_media_info (xmms_decoder_t *decoder)
{
	xmms_transport_t *transport;
	xmms_playlist_entry_t *entry;
	xmms_mad_data_t *data;
	struct id3v1tag_t tag;

	g_return_if_fail (decoder);

	data = xmms_decoder_plugin_data_get (decoder);
	g_return_if_fail (data);

	transport = xmms_decoder_transport_get (decoder);
	g_return_if_fail (transport);

	XMMS_DBG ("Seeking to last 128 bytes");
	xmms_transport_seek (transport, -128, XMMS_TRANSPORT_SEEK_END);
	xmms_transport_read (transport, (gchar *)&tag, 128);
	XMMS_DBG ("Seeking to last first bytes");
	xmms_transport_seek (transport, 0, XMMS_TRANSPORT_SEEK_SET);

	if (strncmp (tag.tag, "TAG", 3) == 0) {
		XMMS_DBG ("Found ID3v1 TAG!");

		entry = g_new0 (xmms_playlist_entry_t, 1);
		sprintf (entry->artist, "%30.30s", tag.artist);
		remove_trail_space (entry->artist);
		sprintf (entry->album, "%30.30s", tag.album);
		remove_trail_space (entry->album);
		sprintf (entry->title, "%30.30s", tag.title);
		remove_trail_space (entry->title);
		entry->year = strtol (tag.year, NULL, 10);
		if (atoi (&tag.u.v1_1.track_number) > 0) {
			/* V1.1 */
			sprintf (entry->comment, "%28.28s", tag.u.v1_1.comment);
			entry->tracknr = atoi (&tag.u.v1_1.track_number);
		} else {
			sprintf (entry->comment, "%30.30s", tag.u.v1_0.comment);
		}

		remove_trail_space (entry->comment);

		decoder->mediainfo = entry;
	}


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

	xmms_decoder_plugin_data_set (decoder, data);
	
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

	if (data->stream.next_frame) {
		gchar *buffer = data->buffer, *nf = data->stream.next_frame;
		memmove (data->buffer, data->stream.next_frame,
				 data->buffer_length = (&buffer[data->buffer_length] - nf));
	} else {
		data->buffer_length = 0;
		xmms_mad_get_media_info (decoder);
	}

	transport = xmms_decoder_transport_get (decoder);
	g_return_val_if_fail (transport, FALSE);
	
	output = xmms_decoder_output_get (decoder);
	g_return_val_if_fail (output, FALSE);

	XMMS_DBG ("Start reading");
	
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

