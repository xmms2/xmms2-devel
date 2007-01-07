/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
#include <string.h> /* for memset() */

#include "xmmsc/xmmsc_idnumbers.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_plugin.h"
#include "xmms/xmms_error.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_streamtype.h"

typedef struct xmms_output_St xmms_output_t;

#define XMMS_OUTPUT_API_VERSION 2

struct xmms_output_plugin_St;
typedef struct xmms_output_plugin_St xmms_output_plugin_t;

typedef struct xmms_output_methods_St {
	gboolean (*new)(xmms_output_t *);
	void (*destroy)(xmms_output_t *);

	gboolean (*open)(xmms_output_t *);
	void (*close)(xmms_output_t *);

	void (*flush)(xmms_output_t *);
	gboolean (*format_set)(xmms_output_t *, const xmms_stream_type_t *);
	gboolean (*format_set_always)(xmms_output_t *, const xmms_stream_type_t *);
	gboolean (*status)(xmms_output_t *, xmms_playback_status_t);

	gboolean (*volume_set)(xmms_output_t *, const gchar *, guint);
	gboolean (*volume_get)(xmms_output_t *, const gchar **, guint *, guint *);

	void (*write)(xmms_output_t *, gpointer, gint, xmms_error_t *);

	guint (*latency_get)(xmms_output_t *);
} xmms_output_methods_t;

#define XMMS_OUTPUT_PLUGIN(shname, name, ver, desc, setupfunc) XMMS_PLUGIN(XMMS_PLUGIN_TYPE_OUTPUT, XMMS_OUTPUT_API_VERSION, shname, name, ver, desc, (gboolean (*)(gpointer))setupfunc)
#define XMMS_OUTPUT_METHODS_INIT(m) memset (&m, 0, sizeof (xmms_output_methods_t))

void xmms_output_plugin_methods_set (xmms_output_plugin_t *output, xmms_output_methods_t *methods);


gpointer xmms_output_private_data_get (xmms_output_t *output);
void xmms_output_private_data_set (xmms_output_t *output, gpointer data);

#define xmms_output_format_add(output, fmt, ch, rate)			\
        xmms_output_stream_type_add (output,                            \
                                     XMMS_STREAM_TYPE_MIMETYPE,         \
                                     "audio/pcm",                       \
                                     XMMS_STREAM_TYPE_FMT_FORMAT,       \
                                     fmt,                               \
                                     XMMS_STREAM_TYPE_FMT_CHANNELS,     \
                                     ch,                                \
                                     XMMS_STREAM_TYPE_FMT_SAMPLERATE,   \
                                     rate,                              \
                                     XMMS_STREAM_TYPE_END)


void xmms_output_stream_type_add (xmms_output_t *output, ...);

gint xmms_output_read (xmms_output_t *output, char *buffer, gint len);
void xmms_output_set_error (xmms_output_t *output, xmms_error_t *error);
guint xmms_output_current_id (xmms_output_t *output, xmms_error_t *error);
gboolean xmms_output_plugin_format_set_always (xmms_output_plugin_t *plugin);

xmms_config_property_t *xmms_output_plugin_config_property_register (xmms_output_plugin_t *plugin, const gchar *name, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);
xmms_config_property_t *xmms_output_config_property_register (xmms_output_t *output, const gchar *name, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);

xmms_config_property_t *xmms_output_config_lookup (xmms_output_t *output, const gchar *path);

#endif
