#ifndef __XMMS_DECODER_H__
#define __XMMS_DECODER_H__

#include "xmms/transport.h"
#include "xmms/output.h"
#include "xmms/playlist.h"

/*
 * Type definitions
 */

/**
 * Structure describing decoder-objects.
 * Do not modify this structure directly, use the functions.
 */
typedef struct xmms_decoder_St {
	xmms_object_t object;

	gboolean running;
	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	/** Mediainfo.
	 *  Stores information about what is beeing decoded right now. 
	 */
	xmms_playlist_entry_t *mediainfo;
	
	xmms_plugin_t *plugin;
	xmms_transport_t *transport; /**< transport associated with decoder.
				      *   This is where the decoder gets it
				      *   data from
				      */

	gpointer plugin_data;

	xmms_output_t *output;       /**< output associated with decoder.
				      *   The decoded data will be written
				      *   to this output.
				      */
} xmms_decoder_t;

/*
 * Decoder plugin methods
 */

typedef gboolean (*xmms_decoder_can_handle_method_t) (const gchar *mimetype);
typedef gboolean (*xmms_decoder_new_method_t) (xmms_decoder_t *decoder,
											   const gchar *mimetype);
typedef gboolean (*xmms_decoder_destroy_method_t) (xmms_decoder_t *decoder);
typedef gboolean (*xmms_decoder_decode_block_method_t) (xmms_decoder_t *decoder);
typedef gboolean (*xmms_decoder_get_mediainfo_method_t) (xmms_decoder_t *decoder);

/*
 * Public function prototypes
 */

gpointer xmms_decoder_plugin_data_get (xmms_decoder_t *decoder);
void xmms_decoder_plugin_data_set (xmms_decoder_t *decoder, gpointer data);

xmms_transport_t *xmms_decoder_transport_get (xmms_decoder_t *decoder);
xmms_output_t *xmms_decoder_output_get (xmms_decoder_t *decoder);

gboolean xmms_decoder_get_mediainfo (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry);
xmms_playlist_entry_t * xmms_decoder_get_mediainfo_offline (xmms_decoder_t *decoder, xmms_transport_t *transport);
void xmms_decoder_set_mediainfo (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry);

xmms_decoder_t *xmms_decoder_new_stacked (xmms_output_t *output, xmms_transport_t *transport, const gchar *mimetype);


/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_decoder_t *xmms_decoder_new (const gchar *mimetype);
void xmms_decoder_destroy (xmms_decoder_t *decoder);

void xmms_decoder_start (xmms_decoder_t *decoder, xmms_transport_t *transport, xmms_output_t *output);

#endif /* __XMMS_DECODER_H__ */
