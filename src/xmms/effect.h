#ifndef _XMMS_EFFECT_H_
#define _XMMS_EFFECT_H_

#include <glib.h>

/*
 * Type definitions
 */

typedef struct xmms_effect_St xmms_effect_t;

void xmms_effect_samplerate_set (xmms_effect_t *effects, guint rate);
void xmms_effect_run (xmms_effect_t *effects, gchar *buf, guint len);

#endif
