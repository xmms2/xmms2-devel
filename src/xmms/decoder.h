#ifndef __XMMS_DECODER_H__
#define __XMMS_DECODER_H__

#include "xmms/transport.h"
#include "xmms/output.h"

/*
 * Type definitions
 */

typedef struct xmms_decoder_St {
	xmms_object_t object;

	gboolean running;
	GThread *thread;
	
	xmms_plugin_t *plugin;
	xmms_transport_t *transport;

	gpointer plugin_data;

	xmms_output_t *output;
} xmms_decoder_t;

/*
 * Decoder plugin methods
 */

typedef gboolean (*xmms_decoder_can_handle_method_t) (const gchar *mimetype);
typedef gboolean (*xmms_decoder_new_method_t) (xmms_decoder_t *decoder,
											   const gchar *mimetype);
typedef gboolean (*xmms_decoder_decode_block_method_t) (xmms_decoder_t *decoder,
														xmms_transport_t *transport);

/*
 * Public function prototypes
 */

gpointer xmms_decoder_plugin_data_get (xmms_decoder_t *decoder);
void xmms_decoder_plugin_data_set (xmms_decoder_t *decoder, gpointer data);

/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_decoder_t *xmms_decoder_new (const gchar *mimetype);

void xmms_decoder_start (xmms_decoder_t *decoder, xmms_transport_t *transport, xmms_output_t *output);

#endif /* __XMMS_DECODER_H__ */
