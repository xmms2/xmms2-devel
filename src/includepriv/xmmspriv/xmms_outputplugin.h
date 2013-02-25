/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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


#ifndef _XMMS_OUTPUTPLUGIN_INT_H_
#define _XMMS_OUTPUTPLUGIN_INT_H_

#include <xmms/xmms_outputplugin.h>
#include <xmmspriv/xmms_plugin.h>

xmms_plugin_t *xmms_output_plugin_new (void);
gboolean xmms_output_plugin_verify (xmms_plugin_t *_plugin);


gboolean xmms_output_plugin_method_new (xmms_output_plugin_t *plugin, xmms_output_t *output);
void xmms_output_plugin_method_destroy (xmms_output_plugin_t *plugin, xmms_output_t *output);
void xmms_output_plugin_method_flush (xmms_output_plugin_t *plugin, xmms_output_t *output);
gboolean xmms_output_plugin_method_format_set (xmms_output_plugin_t *plugin, xmms_output_t *output, xmms_stream_type_t *st);
gboolean xmms_output_plugin_method_status (xmms_output_plugin_t *plugin, xmms_output_t *output, int st);
guint xmms_output_plugin_method_latency_get (xmms_output_plugin_t *plugin, xmms_output_t *output);
gboolean xmms_output_plugin_method_volume_set_available (xmms_output_plugin_t *plugin);
gboolean xmms_output_plugin_methods_volume_set (xmms_output_plugin_t *plugin, xmms_output_t *output, const gchar *chan, guint val);
gboolean xmms_output_plugin_method_volume_get_available (xmms_output_plugin_t *plugin);
gboolean xmms_output_plugin_method_volume_get (xmms_output_plugin_t *plugin, xmms_output_t *output, const gchar **n, guint *x, guint *y);


#endif
