#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "mad_misc.h"
#include <mad.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>

/*
 * Type definitions
 */


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
static gboolean xmms_mad_decode_block (xmms_decoder_t *decoder, xmms_transport_t *transport);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "MAD decoder " VERSION,
			"MPEG Layer 1/2/3 decoder");

	xmms_plugin_method_add (plugin, "can_handle", xmms_mad_can_handle);
	xmms_plugin_method_add (plugin, "new", xmms_mad_new);
	xmms_plugin_method_add (plugin, "decode_block", xmms_mad_decode_block);

	return plugin;
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
xmms_mad_decode_block (xmms_decoder_t *decoder, xmms_transport_t *transport)
{
	xmms_mad_data_t *data;
	gchar out[1152 * 4];
	mad_fixed_t *ch1, *ch2;
	mad_fixed_t clipping;
	gulong clipped;
	gint ret;

	data = xmms_decoder_plugin_data_get (decoder);

	if (data->stream.next_frame) {
		gchar *buffer = data->buffer, *nf = data->stream.next_frame;
		memmove (data->buffer, data->stream.next_frame,
				 data->buffer_length = (&buffer[data->buffer_length] - nf));
	} else {
		data->buffer_length = 0;
	}
	
	ret = xmms_transport_read (transport, data->buffer + data->buffer_length,
						 4096 - data->buffer_length);
	if (ret > 0)
		data->buffer_length += ret;
	mad_stream_buffer (&data->stream, data->buffer, data->buffer_length);
		
	for (;;) {
		if (mad_frame_decode (&data->frame, &data->stream) == -1) {
			if (!MAD_RECOVERABLE (data->stream.error))
				break;
			
			g_warning ("Error %d in frame", data->stream.error);
		}
		
		mad_synth_frame (&data->synth, &data->frame);
		
		ch1 = data->synth.pcm.samples[0];
		ch2 = data->synth.pcm.samples[1];
		
		ret = pack_pcm (out, data->synth.pcm.length, ch1, ch2, 16, &clipped, &clipping);
		xmms_output_write (decoder->output, out, sizeof(out));
	}
	
	return TRUE;
}

