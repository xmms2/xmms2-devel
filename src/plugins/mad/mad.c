#include "xmms/plugin.h"
#include "mad_misc.h"
#include <mad.h>

#include <glib.h>

/*
 * definitions
 */

typedef struct xmms_plugin_data_St {
	void *data;
} xmms_plugin_data_t;

typedef struct xmms_mad_data_St {
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;
} xmms_mad_data_t;



static gboolean xmms_mad_can_handle (const gchar *mimetype);
static xmms_plugin_data_t* xmms_mad_init ();
static gchar *xmms_mad_decode_chunk (xmms_plugin_data_t *data, gint chunk_len, gchar *chunk);
static gchar *xmms_mad_decode_flush ();
static void xmms_mad_deinit ();


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
	xmms_plugin_method_add (plugin, "init", xmms_mad_init);
	xmms_plugin_method_add (plugin, "decode_chunk", xmms_mad_decode_chunk);
	xmms_plugin_method_add (plugin, "decode_flush", xmms_mad_decode_flush);
	xmms_plugin_method_add (plugin, "deinit", xmms_mad_deinit);

	return plugin;
}

static gboolean
xmms_mad_can_handle (const gchar *mimetype)
{

	if ((g_strncasecmp (mimetype, "audio/mpeg", 10) == 0))
		return TRUE;

	return FALSE;

}

static xmms_plugin_data_t* 
xmms_mad_init (/*här får man nog något bra*/)
{
	xmms_mad_data_t *mad_data;
	xmms_plugin_data_t *data;

	mad_data = g_new0 (xmms_mad_data_t, 1);

	mad_stream_init (&mad_data->stream);
	mad_frame_init (&mad_data->frame);
	mad_synth_init (&mad_data->synth);

	data = g_new0 (xmms_plugin_data_t, 1);
	data->data = (void*) mad_data;

	return data;

}

static gchar *
xmms_mad_decode_chunk (xmms_plugin_data_t *data, gint chunk_len, gchar *chunk)
{
	xmms_mad_data_t *mad_data = (xmms_mad_data_t *)data->data;
	gchar out[576*4];
	mad_fixed_t ch1, ch2;
	mad_fixed_t o_len, clipping;
	unsigned long clipped;

	mad_stream_buffer (&mad_data->stream, chunk, chunk_len);

	if (mad_frame_decode (&mad_data->frame, &mad_data->stream) == -1) {
		if (!MAD_RECOVERABLE (mad_data->stream.error))
			return NULL;

		g_warning ("Error %d in frame", mad_data->stream.error);
	}

	mad_synth_frame (&mad_data->synth, &mad_data->frame);
	
	ch1 = mad_data->synth.pcm.samples[0];
	ch2 = mad_data->synth.pcm.samples[1];

	o_len += pack_pcm (out, mad_data->synth.pcm.length, ch1, ch2, 16, &clipped, &clipping);

	return out;
}

static gchar *
xmms_mad_decode_flush ()
{

	gchar *out;

	return out;

}

static void
xmms_mad_deinit ()
{

	/*free som fan*/

}

