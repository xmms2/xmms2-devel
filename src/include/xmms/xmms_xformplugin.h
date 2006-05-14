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




#ifndef __XMMS_XFORMPLUGIN_H__
#define __XMMS_XFORMPLUGIN_H__

#include <glib.h>
#include <string.h>



#define XMMS_XFORM_API_VERSION 1

/*
 * Type definitions
 */


#include "xmms/xmms_error.h"
#include "xmms/xmms_plugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_streamtype.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_medialib.h"


struct xmms_xform_plugin_St;
typedef struct xmms_xform_plugin_St xmms_xform_plugin_t;

#define XMMS_XFORM_PLUGIN(shname, name, ver, desc, setupfunc) XMMS_PLUGIN(XMMS_PLUGIN_TYPE_XFORM, XMMS_XFORM_API_VERSION, shname, name, ver, desc, (gboolean (*)(gpointer))setupfunc)

/* */
#define XMMS_XFORM_DATA_SIZE "size"
#define XMMS_XFORM_DATA_LMOD "lmod"

struct xmms_xform_St;
typedef struct xmms_xform_St xmms_xform_t;

typedef enum xmms_xform_seek_mode_E {
	XMMS_XFORM_SEEK_CUR = 1,
	XMMS_XFORM_SEEK_SET = 2,
	XMMS_XFORM_SEEK_END = 3
} xmms_xform_seek_mode_t;

typedef struct xmms_xform_methods_St {
	gboolean (*init)(xmms_xform_t *);
	void (*destroy)(xmms_xform_t *);
	gint (*read)(xmms_xform_t *, gpointer, gint, xmms_error_t *);
	gint64 (*seek)(xmms_xform_t *, gint64, xmms_xform_seek_mode_t, xmms_error_t *);
} xmms_xform_methods_t;

#define XMMS_XFORM_METHODS_INIT(m) memset (&m, 0, sizeof (xmms_xform_methods_t))



void xmms_xform_plugin_methods_set (xmms_xform_plugin_t *plugin, xmms_xform_methods_t *methods);
void xmms_xform_plugin_indata_add (xmms_xform_plugin_t *plugin, ...);

void xmms_xform_ringbuf_resize (xmms_xform_t *xform, gint size);
gpointer xmms_xform_private_data_get (xmms_xform_t *xform);
void xmms_xform_private_data_set (xmms_xform_t *xform, gpointer data);


void xmms_xform_outdata_type_add (xmms_xform_t *xform, ...);
void xmms_xform_outdata_type_copy (xmms_xform_t *xform);

void xmms_xform_metadata_set_int (xmms_xform_t *xform, const gchar *key, int val);
void xmms_xform_metadata_set_str (xmms_xform_t *xform, const gchar *key, const char *val);

gboolean xmms_xform_metadata_has_val (xmms_xform_t *xform, const gchar *key);
gint xmms_xform_metadata_get_int (xmms_xform_t *xform, const gchar *key);
const gchar *xmms_xform_metadata_get_str (xmms_xform_t *xform, const gchar *key);


const char *xmms_xform_indata_get_str (xmms_xform_t *xform, xmms_stream_type_key_t key);
gint xmms_xform_indata_get_int (xmms_xform_t *xform, xmms_stream_type_key_t key);


gint xmms_xform_peek (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err);
gint xmms_xform_read (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err);
gint64 xmms_xform_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *err);
gboolean xmms_xform_iseos (xmms_xform_t *xform);

gboolean xmms_magic_add (const gchar *desc, const gchar *mime, ...);

xmms_config_property_t *xmms_xform_plugin_config_property_register (
	xmms_xform_plugin_t *xform_plugin,
	const gchar *name,
	const gchar *default_value,
	xmms_object_handler_t cb,
	gpointer userdata
);
xmms_config_property_t *xmms_xform_config_lookup (xmms_xform_t *xform,
                                                  const gchar *path);

xmms_medialib_entry_t xmms_xform_entry_get (xmms_xform_t *xform);

#endif
