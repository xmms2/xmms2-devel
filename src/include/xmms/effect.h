#ifndef _XMMS_EFFECT_H_
#define _XMMS_EFFECT_H_

#include <glib.h>
#include "xmms/config.h"
#include "xmms/plugin.h"

/*
 * Type definitions
 */

typedef struct xmms_effect_St xmms_effect_t;

void xmms_effect_samplerate_set (xmms_effect_t *effects, guint rate);
void xmms_effect_run (xmms_effect_t *effects, gchar *buf, guint len);
gpointer xmms_effect_plugin_data_get (xmms_effect_t *effect);
void xmms_effect_plugin_data_set (xmms_effect_t *effect, gpointer data);
xmms_config_value_t *xmms_effect_config_value_get (xmms_effect_t *effect, gchar *key, gchar *def);
xmms_plugin_t * xmms_effect_plugin_get (xmms_effect_t *effect);

/* _int */
xmms_effect_t *xmms_effect_prepend (xmms_effect_t *stack, gchar *name);

#endif
