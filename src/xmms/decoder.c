#include "decoder.h"
#include "plugin.h"
#include "object.h"
#include "util.h"
#include "output.h"
#include "transport.h"
#include "ringbuf.h"

/*
 * Type definitions
 */

/*
 * Static function prototypes
 */

static xmms_plugin_t *xmms_decoder_find_plugin (const gchar *mimetype);
static gpointer xmms_decoder_thread (gpointer data);

/*
 * Public functions
 */

gpointer
xmms_decoder_plugin_data_get (xmms_decoder_t *decoder)
{
	g_return_val_if_fail (decoder, NULL);

	return decoder->plugin_data;
}

void
xmms_decoder_plugin_data_set (xmms_decoder_t *decoder, gpointer data)
{
	decoder->plugin_data = data;
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
	decoder->eos_cond = g_cond_new ();

	new_method = xmms_plugin_method_get (plugin, "new");

	if (!new_method || !new_method (decoder, mimetype)) {
		XMMS_DBG ("new failed");
		g_free (decoder);
	}
	
	return decoder;
}

void
xmms_decoder_start (xmms_decoder_t *decoder, xmms_transport_t *transport, xmms_output_t *output)
{
	g_return_if_fail (decoder);
	g_return_if_fail (output);

	decoder->running = TRUE;
	decoder->transport = transport;
	decoder->output = output;
	decoder->thread = g_thread_create (xmms_decoder_thread, decoder, FALSE, NULL); 
}

/*
 * Static functions
 */

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
	
	while (decoder->running) {
		decode_block (decoder, decoder->transport);
	}

	XMMS_DBG ("Decoder thread quiting");

	return NULL;
}
