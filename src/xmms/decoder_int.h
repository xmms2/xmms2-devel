#ifndef __XMMS_DECODER_INT_H__
#define __XMMS_DECODER_INT_H__

#include "xmms/transport.h"
#include "xmms/output.h"
#include "xmms/effect.h"

/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_decoder_t *xmms_decoder_new (void);
gboolean xmms_decoder_open (xmms_decoder_t *decoder,
			    xmms_playlist_entry_t *entry);
void xmms_decoder_start (xmms_decoder_t *decoder, 
			xmms_transport_t *transport, 
			xmms_effect_t *effect, 
			xmms_output_t *output);
void xmms_decoder_seek_ms (xmms_decoder_t *decoder, guint milliseconds);
void xmms_decoder_seek_samples (xmms_decoder_t *decoder, guint samples);
void xmms_decoder_destroy (xmms_decoder_t *decoder);
void xmms_decoder_wait (xmms_decoder_t *decoder);

#endif /* __XMMS_DECODER_INT_H__ */
