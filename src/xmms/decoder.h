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
typedef struct xmms_decoder_St xmms_decoder_t;

/*
 * Decoder plugin methods
 */

typedef gboolean (*xmms_decoder_can_handle_method_t) (const gchar *mimetype);
typedef gboolean (*xmms_decoder_init_method_t) (xmms_decoder_t *decoder);
typedef gboolean (*xmms_decoder_new_method_t) (xmms_decoder_t *decoder,
											   const gchar *mimetype);
typedef gboolean (*xmms_decoder_destroy_method_t) (xmms_decoder_t *decoder);
typedef gboolean (*xmms_decoder_decode_block_method_t) (xmms_decoder_t *decoder);
typedef xmms_playlist_entry_t *(*xmms_decoder_get_mediainfo_method_t) (xmms_decoder_t *decoder);

/*
 * Public function prototypes
 */

gpointer xmms_decoder_plugin_data_get (xmms_decoder_t *decoder);
void xmms_decoder_plugin_data_set (xmms_decoder_t *decoder, gpointer data);

xmms_transport_t *xmms_decoder_transport_get (xmms_decoder_t *decoder);
xmms_output_t *xmms_decoder_output_get (xmms_decoder_t *decoder);
xmms_plugin_t *xmms_decoder_plugin_get (xmms_decoder_t *);

gboolean xmms_decoder_get_mediainfo (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry);
xmms_playlist_entry_t * xmms_decoder_get_mediainfo_offline (xmms_decoder_t *decoder, xmms_transport_t *transport);
void xmms_decoder_set_mediainfo (xmms_decoder_t *decoder, xmms_playlist_entry_t *entry);

void xmms_decoder_write (xmms_decoder_t *decoder, gchar *buf, guint len);
void xmms_decoder_samplerate_set (xmms_decoder_t *decoder, guint rate);


xmms_decoder_t *xmms_decoder_new_stacked (xmms_output_t *output, xmms_transport_t *transport, const gchar *mimetype);
void xmms_decoder_destroy (xmms_decoder_t *decoder);


#endif /* __XMMS_DECODER_H__ */
