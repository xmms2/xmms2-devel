#ifndef __XMMS_DECODER_INT_H__
#define __XMMS_DECODER_INT_H__

#include "xmms/transport.h"
#include "xmms/output.h"
#include "xmms/effect.h"

/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_decoder_t *xmms_decoder_new (xmms_playlist_entry_t *entry);

void xmms_decoder_start (xmms_decoder_t *decoder, xmms_transport_t *transport, xmms_effect_t *effect, xmms_output_t *output);
void xmms_decoder_seek_ms (xmms_decoder_t *decoder, guint milliseconds);
void xmms_decoder_seek_samples (xmms_decoder_t *decoder, guint samples);

#endif /* __XMMS_DECODER_INT_H__ */
