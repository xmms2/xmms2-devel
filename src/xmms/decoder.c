#include "decoder.h"
#include "plugin.h"
#include "object.h"
#include "util.h"
#include "output.h"
#include "transport.h"
#include "ringbuf.h"
#include "playlist.h"

#include <string.h>

/*
 * Static function prototypes
 */

static void xmms_decoder_destroy_real (xmms_decoder_t *decoder);
static xmms_plugin_t *xmms_decoder_find_plugin (const gchar *mimetype);
static gpointer xmms_decoder_thread (gpointer data);
static void xmms_decoder_output_eos (xmms_object_t *object, gconstpointer data, gpointer userdata);

/*
 * Macros
 */

#define xmms_decoder_lock(d) g_mutex_lock ((d)->mutex)
#define xmms_decoder_unlock(d) g_mutex_unlock ((d)->mutex)

/*
 * Public functions
 */

gpointer
xmms_decoder_plugin_data_get (xmms_decoder_t *decoder)
{
	gpointer ret;
	g_return_val_if_fail (decoder, NULL);

	xmms_decoder_lock (decoder);
	ret = decoder->plugin_data;
	xmms_decoder_unlock (decoder);

	return ret;
}

void
xmms_decoder_plugin_data_set (xmms_decoder_t *decoder, gpointer data)
{
	g_return_if_fail (decoder);

	xmms_decoder_lock (decoder);
	decoder->plugin_data = data;
	xmms_decoder_unlock (decoder);
}

xmms_transport_t *
xmms_decoder_transport_get (xmms_decoder_t *decoder)
{
	xmms_transport_t *ret;
	g_return_val_if_fail (decoder, NULL);

	xmms_decoder_lock (decoder);
	ret = decoder->transport;
	xmms_decoder_unlock (decoder);

	return ret;
}

xmms_output_t *
xmms_decoder_output_get (xmms_decoder_t *decoder)
{
	xmms_output_t *ret;
	g_return_val_if_fail (decoder, NULL);

	xmms_decoder_lock (decoder);
	ret = decoder->output;
	xmms_decoder_unlock (decoder);

	return ret;
}

/*
 * Private functions
 */

xmms_decoder_t *
xmms_decoder_new (const gchar *mimetype)
{
	xmms_plugin_t *plugin;
	xmms_decoder_t *decoder;
	xmms_decoder_new_method_t new_method;

	g_return_val_if_fail (mimetype, NULL);

	XMMS_DBG ("Trying to create decoder for mime-type %s", mimetype);
	
	plugin = xmms_decoder_find_plugin (mimetype);
	if (!plugin)
		return NULL;
	
	XMMS_DBG ("Found plugin: %s", xmms_plugin_name_get (plugin));

	decoder = g_new0 (xmms_decoder_t, 1);
	xmms_object_init (XMMS_OBJECT (decoder));
	decoder->plugin = plugin;
	decoder->mutex = g_mutex_new ();
	decoder->cond = g_cond_new ();

	new_method = xmms_plugin_method_get (plugin, "new");

	if (!new_method || !new_method (decoder, mimetype)) {
		XMMS_DBG ("new failed");
		g_free (decoder);
	}
	
	return decoder;
}

void
xmms_decoder_destroy (xmms_decoder_t *decoder)
{
	g_return_if_fail (decoder);

	if (decoder->thread) {
		xmms_decoder_lock (decoder);
		decoder->running = FALSE;
		g_cond_signal (decoder->cond);
		xmms_decoder_unlock (decoder);
	} else {
		xmms_decoder_destroy_real (decoder);
	}
		
}

void
xmms_decoder_start (xmms_decoder_t *decoder, xmms_transport_t *transport, xmms_output_t *output)
{
	g_return_if_fail (decoder);
	g_return_if_fail (output);

	decoder->running = TRUE;
	decoder->transport = transport;
	decoder->output = output;
	xmms_output_set_eos (output, FALSE);
	xmms_object_connect (XMMS_OBJECT (output), "eos-reached",
						 xmms_decoder_output_eos, decoder);
	decoder->thread = g_thread_create (xmms_decoder_thread, decoder, FALSE, NULL); 
}

gboolean
xmms_decoder_get_mediainfo (xmms_decoder_t *decoder, 
			xmms_playlist_entry_t *entry)
{
	xmms_playlist_entry_t *mediainfo;
	xmms_decoder_get_media_info_method_t get_media_info;

	g_return_val_if_fail (decoder, FALSE);
	g_return_val_if_fail (entry, FALSE);

	get_media_info = xmms_plugin_method_get (decoder->plugin, "get_media_info");
	if (get_media_info)
		get_media_info (decoder);

	g_return_val_if_fail (decoder->mediainfo, FALSE);

	mediainfo = decoder->mediainfo;
	
	if (mediainfo->artist)
		memcpy (entry->artist, mediainfo->artist, XMMS_PL_PROPERTY);
	if (mediainfo->album)
		memcpy (entry->album, mediainfo->album, XMMS_PL_PROPERTY);
	if (mediainfo->title)
		memcpy (entry->title, mediainfo->title, XMMS_PL_PROPERTY);
	if (mediainfo->comment)
		memcpy (entry->comment, mediainfo->comment, XMMS_PL_PROPERTY);
	if (mediainfo->genre)
		memcpy (entry->genre, mediainfo->genre, XMMS_PL_PROPERTY);

	entry->year = mediainfo->year;
	entry->tracknr = mediainfo->tracknr;

	return TRUE;
}


/*
 * Static functions
 */

static void
xmms_decoder_destroy_real (xmms_decoder_t *decoder)
{
	xmms_decoder_destroy_method_t destroy_method;
	
	g_return_if_fail (decoder);
	
	destroy_method = xmms_plugin_method_get (decoder->plugin, "destroy");
	if (destroy_method)
		destroy_method (decoder);

	g_cond_free (decoder->cond);
	g_mutex_free (decoder->mutex);
	g_free (decoder);
}

static xmms_plugin_t *
xmms_decoder_find_plugin (const gchar *mimetype)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;
	xmms_decoder_can_handle_method_t can_handle;

	g_return_val_if_fail (mimetype, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_DECODER);
	XMMS_DBG ("List: %p", list);
	
	for (node = list; node; node = g_list_next (node)) {
		plugin = node->data;
		XMMS_DBG ("Trying plugin: %s", xmms_plugin_name_get (plugin));
		can_handle = xmms_plugin_method_get (plugin, "can_handle");
		
		if (!can_handle)
			continue;

		if (can_handle (mimetype)) {
			xmms_plugin_ref (plugin);
			break;
		}
	}
	if (!node)
		plugin = NULL;

	if (list)
		xmms_plugin_list_destroy (list);

	return plugin;
}



static gpointer
xmms_decoder_thread (gpointer data)
{
	xmms_decoder_t *decoder = data;
	xmms_decoder_decode_block_method_t decode_block;

	g_return_val_if_fail (decoder, NULL);
	
	decode_block = xmms_plugin_method_get (decoder->plugin, "decode_block");
	if (!decode_block)
		return NULL;

	xmms_decoder_lock (decoder);
	while (decoder->running) {
		gboolean ret;
		
		xmms_decoder_unlock (decoder);
		ret = decode_block (decoder);
		xmms_decoder_lock (decoder);
		
		if (!ret) {
			xmms_output_set_eos (decoder->output, TRUE);
			g_cond_wait (decoder->cond, decoder->mutex);
		}
	}
	g_mutex_unlock (decoder->mutex);

	xmms_decoder_destroy_real (decoder);

	XMMS_DBG ("Decoder thread quiting");



	return NULL;
}

static void
xmms_decoder_output_eos (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_object_emit (XMMS_OBJECT (userdata), "eos-reached", NULL);
}
