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




#ifndef _XMMS_OUTPUT_INT_H_
#define _XMMS_OUTPUT_INT_H_

#include "xmms/plugin.h"
#include "xmms/config.h"

/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_output_t * xmms_output_new (xmms_core_t *core, xmms_plugin_t *plugin);
gboolean xmms_output_open (xmms_output_t *output);
void xmms_output_close (xmms_output_t *output);
void xmms_output_start (xmms_output_t *output);
void xmms_output_set_eos (xmms_output_t *output, gboolean eos);
xmms_plugin_t * xmms_output_find_plugin ();
void xmms_output_write (xmms_output_t *output, gpointer buffer, gint len);
void xmms_output_samplerate_set (xmms_output_t *output, guint rate);
void xmms_output_played_samples_set (xmms_output_t *output, guint samples);

#endif
