/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *                   
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */




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
gpointer xmms_effect_private_data_get (xmms_effect_t *effect);
void xmms_effect_private_data_set (xmms_effect_t *effect, gpointer data);
xmms_config_value_t *xmms_effect_config_value_get (xmms_effect_t *effect, gchar *key, gchar *def);
xmms_plugin_t * xmms_effect_plugin_get (xmms_effect_t *effect);

/* _int */
xmms_effect_t *xmms_effect_prepend (xmms_effect_t *stack, gchar *name);

#endif
