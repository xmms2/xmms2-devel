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




#ifndef _XMMS_OUTPUTPLUGIN_H_
#define _XMMS_OUTPUTPLUGIN_H_

#include <glib.h>

#include "xmmsc/xmmsc_idnumbers.h"
#include "xmms/xmms_plugin.h"
#include "xmms/xmms_output.h"

/*
 * Output plugin methods
 */

typedef void (*xmms_output_status_method_t) (xmms_output_t *output, xmms_playback_status_t status);
typedef void (*xmms_output_write_method_t) (xmms_output_t *output, gchar *buffer, gint len);
typedef void (*xmms_output_destroy_method_t) (xmms_output_t *output);
typedef gboolean (*xmms_output_open_method_t) (xmms_output_t *output);
typedef gboolean (*xmms_output_new_method_t) (xmms_output_t *output);
typedef gboolean (*xmms_output_volume_get_method_t) (xmms_output_t *output, const gchar **names, guint *values, guint *num_channels);
typedef gboolean (*xmms_output_volume_set_method_t) (xmms_output_t *output, const gchar *channel, guint volume);
typedef void (*xmms_output_flush_method_t) (xmms_output_t *output);
typedef void (*xmms_output_close_method_t) (xmms_output_t *output);
typedef gboolean (*xmms_output_format_set_method_t) (xmms_output_t *output, xmms_audio_format_t *fmt);
typedef guint (*xmms_output_buffersize_get_method_t) (xmms_output_t *output);

#define XMMS_PLUGIN_METHOD_WRITE_TYPE xmms_output_write_method_t
#define XMMS_PLUGIN_METHOD_OPEN_TYPE xmms_output_open_method_t
#define XMMS_PLUGIN_METHOD_NEW_TYPE xmms_output_new_method_t
#define XMMS_PLUGIN_METHOD_DESTROY_TYPE xmms_output_destroy_method_t
#define XMMS_PLUGIN_METHOD_CLOSE_TYPE xmms_output_close_method_t
#define XMMS_PLUGIN_METHOD_FLUSH_TYPE xmms_output_flush_method_t
#define XMMS_PLUGIN_METHOD_BUFFERSIZE_GET_TYPE xmms_output_buffersize_get_method_t
#define XMMS_PLUGIN_METHOD_FORMAT_SET_TYPE xmms_output_format_set_method_t
#define XMMS_PLUGIN_METHOD_VOLUME_GET_TYPE xmms_output_volume_get_method_t
#define XMMS_PLUGIN_METHOD_VOLUME_SET_TYPE xmms_output_volume_set_method_t
#define XMMS_PLUGIN_METHOD_STATUS_TYPE xmms_output_status_method_t

xmms_plugin_t *xmms_output_plugin_get (xmms_output_t *output);
gpointer xmms_output_private_data_get (xmms_output_t *output);
void xmms_output_private_data_set (xmms_output_t *output, gpointer data);

void xmms_output_format_add (xmms_output_t *output, xmms_sample_format_t fmt, guint channels, guint rate);

gint xmms_output_read (xmms_output_t *output, char *buffer, gint len);

#endif
