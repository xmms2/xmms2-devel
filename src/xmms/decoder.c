/** @file
 * Decoder.
 *
 * This file contains functions that manipulate xmms_decoder_t objects.
 *
 */


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

	ret = decoder->plugin_data;

	return ret;
}

void
xmms_decoder_plugin_data_set (xmms_decoder_t *decoder, gpointer data)
{
	g_return_if_fail (decoder);

	decoder->plugin_data = data;
}

/**
 * Get the transport associated with the decoder.
 *
 * @param decoder apanap
 * @return the associated transport
 */
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

/**
 * Get the output associated with the decoder.
 */
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


/**
 * Creates a new decoder for the specified mimetype.
 * This call creates a decoder that can handle the specified mimetype.
 * The transport is started but not the output, which must be driven by
 * hand. @sa tar.c for example.
 */
xmms_decoder_t *
xmms_decoder_new_stacked (xmms_output_t *output, xmms_transport_t *transport, const gchar *mimetype){
	xmms_decoder_t *decoder;
	decoder = xmms_decoder_new (mimetype);
	decoder->transport = transport;
	decoder->output = output;
	XMMS_DBG ("starting threads..");
	xmms_transport_start (transport);
	XMMS_DBG ("transport started");
	return decoder;
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

	new_method = xmms_plugin_method_get (plugin, XMMS_METHOD_NEW);

	if (!new_method || !new_method (decoder, mimetype)) {
		XMMS_DBG ("new failed");
		g_free (decoder);
	}
	
	return decoder;
}


/**
 * Free resources used by decoder.
 *
 * The decoder is signalled to stop working and free up any resources
 * it has been using.
 *
 * @param decoder the decoder to free. After this called it must not be referenced again.
 */
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

/**
 * Dispatch execution of the decoder.
 *
 * Associates the decoder with a transport and output.
 * Blesses it with a life of its own (a new thread is created)
 *
 * @param decoder
 * @param transport
 * @param output
 *
 */
void
xmms_decoder_start (xmms_decoder_t *decoder, xmms_transport_t *transport, xmms_output_t *output)
{
	g_return_if_fail (decoder);
	g_return_if_fail (output);

	decoder->running = TRUE;
	decoder->transport = transport;
	decoder->output = output;
	xmms_output_set_eos (output, FALSE);
	decoder->thread = g_thread_create (xmms_decoder_thread, decoder, FALSE, NULL); 
}


/**
 * Sets information about what is beeing decoded.
 *
 * The caller is responsibe to keep the entry around as long
 * as the decoder exists or the mediainfo is changed to something else. 
 *
 * @param decoder 
 * @param entry the information to set.
 */
void
xmms_decoder_set_mediainfo (xmms_decoder_t *decoder,
			xmms_playlist_entry_t *entry)
{
	decoder->mediainfo = entry;
	xmms_object_emit (XMMS_OBJECT (decoder), "mediainfo-changed", decoder);
}

/**
 * Get a copy of structure describing what is beeing decoded.
 *
 * @param decoder
 * @param entry 
 *
 * @return TRUE if entry was successfully filled in with information, 
 * FALSE otherwise.
 */
gboolean
xmms_decoder_get_mediainfo (xmms_decoder_t *decoder, 
			xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (decoder, FALSE);
	g_return_val_if_fail (entry, FALSE);

	g_return_val_if_fail (decoder->mediainfo, FALSE);

	xmms_playlist_entry_copy_property (decoder->mediainfo, entry);
	
	return TRUE;
}

xmms_playlist_entry_t *
xmms_decoder_get_mediainfo_offline (xmms_decoder_t *decoder, 
			xmms_transport_t *transport)
{
	xmms_playlist_entry_t *entry;
	xmms_decoder_get_mediainfo_method_t mediainfo;
	g_return_val_if_fail (decoder, NULL);
	g_return_val_if_fail (transport, NULL);

	decoder->transport = transport;

	mediainfo = xmms_plugin_method_get (decoder->plugin, XMMS_METHOD_GET_MEDIAINFO);
	if (!mediainfo) {
		XMMS_DBG ("get_mediainfo failed");
		return NULL;
	}

	entry = mediainfo (decoder);

	return entry;
}

/*
 * Static functions
 */

static void
xmms_decoder_destroy_real (xmms_decoder_t *decoder)
{
	xmms_decoder_destroy_method_t destroy_method;
	
	g_return_if_fail (decoder);
	
	destroy_method = xmms_plugin_method_get (decoder->plugin, XMMS_METHOD_DESTROY);
	if (destroy_method)
		destroy_method (decoder);

	g_cond_free (decoder->cond);
	g_mutex_free (decoder->mutex);
	if (decoder->mediainfo)
		xmms_playlist_entry_free (decoder->mediainfo);
	xmms_object_cleanup (XMMS_OBJECT (decoder));
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
		can_handle = xmms_plugin_method_get (plugin, XMMS_METHOD_CAN_HANDLE);
		
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
	
	decode_block = xmms_plugin_method_get (decoder->plugin, XMMS_METHOD_DECODE_BLOCK);
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
