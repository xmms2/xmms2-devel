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
#include "xmms/output.h"

/*
 * Type definitions
 */

typedef struct xmms_effect_St xmms_effect_t;

gboolean xmms_effect_format_set (xmms_effect_t *effect, xmms_audio_format_t *fmt);
void xmms_effect_run (xmms_effect_t *effect, xmms_sample_t *buf, guint len);
gpointer xmms_effect_private_data_get (xmms_effect_t *effect);
void xmms_effect_private_data_set (xmms_effect_t *effect, gpointer data);
xmms_plugin_t * xmms_effect_plugin_get (xmms_effect_t *effect);

/* _int */
xmms_effect_t *xmms_effect_new (xmms_plugin_t *plugin, xmms_output_t *output);
void xmms_effect_free (xmms_effect_t *effect);

#endif
