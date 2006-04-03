/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

typedef struct xmms_output_St xmms_output_t;

#define XMMS_OUTPUT_API_VERSION 1

struct xmms_output_plugin_St;
typedef struct xmms_output_plugin_St xmms_output_plugin_t;

typedef struct xmms_output_methods_St {
	gboolean (*new)(xmms_output_t *);
	void (*destroy)(xmms_output_t *);

	gboolean (*open)(xmms_output_t *);
	void (*close)(xmms_output_t *);

	void (*flush)(xmms_output_t *);
	gboolean (*format_set)(xmms_output_t *, const xmms_audio_format_t *);
	gboolean (*status)(xmms_output_t *, int);

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

void xmms_output_format_add (xmms_output_t *output, xmms_sample_format_t fmt, guint channels, guint rate);

gint xmms_output_read (xmms_output_t *output, char *buffer, gint len);
void xmms_output_set_error (xmms_output_t *output, xmms_error_t *error);


xmms_config_property_t *xmms_output_plugin_config_property_register (xmms_output_plugin_t *plugin, const gchar *name, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);
xmms_config_property_t *xmms_output_config_property_register (xmms_output_t *output, const gchar *name, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);

xmms_config_property_t *xmms_output_config_lookup (xmms_output_t *output, const gchar *path);

#endif
