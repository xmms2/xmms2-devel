/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 Peter Alm, Tobias Rundström, Anders Gustafsson
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




#ifndef __XMMS_PLUGIN_H__
#define __XMMS_PLUGIN_H__

#include <glib.h>

typedef struct xmms_plugin_St xmms_plugin_t;

#include "xmms/xmms_object.h"
#include "xmms/xmms_config.h"
#include "xmmsc/xmmsc_idnumbers.h"

/* API VERSIONS */
#define XMMS_TRANSPORT_PLUGIN_API_VERSION 3
#define XMMS_DECODER_PLUGIN_API_VERSION 2
#define XMMS_EFFECT_PLUGIN_API_VERSION 2
#define XMMS_PLAYLIST_PLUGIN_API_VERSION 2
#define XMMS_OUTPUT_PLUGIN_API_VERSION 2


/* 
 * Plugin methods
 */

#define XMMS_PLUGIN_METHOD_CAN_HANDLE "can_handle"
#define XMMS_PLUGIN_METHOD_OPEN "open"
#define XMMS_PLUGIN_METHOD_CLOSE "close"
#define XMMS_PLUGIN_METHOD_READ "read"
#define XMMS_PLUGIN_METHOD_SIZE "size"
#define XMMS_PLUGIN_METHOD_SEEK "seek"
#define XMMS_PLUGIN_METHOD_NEW "new"
#define XMMS_PLUGIN_METHOD_BUFFERSIZE_GET "buffersize_get"
#define XMMS_PLUGIN_METHOD_DECODE_BLOCK "decode_block"
#define XMMS_PLUGIN_METHOD_GET_MEDIAINFO "get_mediainfo"
#define XMMS_PLUGIN_METHOD_DESTROY "destroy"
#define XMMS_PLUGIN_METHOD_WRITE "write"
#define XMMS_PLUGIN_METHOD_SEARCH "search"
#define XMMS_PLUGIN_METHOD_CALC_SKIP "calc_skip"
#define XMMS_PLUGIN_METHOD_ADD_ENTRY "add_entry"
#define XMMS_PLUGIN_METHOD_INIT "init"
#define XMMS_PLUGIN_METHOD_READ_PLAYLIST "read_playlist"
#define XMMS_PLUGIN_METHOD_WRITE_PLAYLIST "write_playlist"
#define XMMS_PLUGIN_METHOD_PROCESS "process"
#define XMMS_PLUGIN_METHOD_FLUSH "flush"
#define XMMS_PLUGIN_METHOD_VOLUME_GET "volume_get"
#define XMMS_PLUGIN_METHOD_VOLUME_SET "volume_set"
#define XMMS_PLUGIN_METHOD_LIST "list"
#define XMMS_PLUGIN_METHOD_STATUS "status"
#define XMMS_PLUGIN_METHOD_LMOD "lmod"
#define XMMS_PLUGIN_METHOD_FORMAT_SET "format_set"
#define XMMS_PLUGIN_METHOD_CURRENT_MEDIALIB_ENTRY "current_medialib_entry"

/*
 * Plugin properties.
 */

/* For transports */
#define XMMS_PLUGIN_PROPERTY_LOCAL (1 << 0)
#define XMMS_PLUGIN_PROPERTY_LIST (1 << 1)
#define XMMS_PLUGIN_PROPERTY_STREAM (1 << 2)

/* For decoders */
#define XMMS_PLUGIN_PROPERTY_FAST_FWD (1 << 4)
#define XMMS_PLUGIN_PROPERTY_REWIND (1 << 5)
#define XMMS_PLUGIN_PROPERTY_SUBTUNES (1 << 6)

/* For output */

/*
 * Type declarations
 */

typedef void *xmms_plugin_method_t;

/*
 * Public functions
 */

xmms_plugin_t *xmms_plugin_new (xmms_plugin_type_t type, 
				gint api_version,
				const gchar *shortname,
				const gchar *name,
				const gchar *version,
				const gchar *description);

#define xmms_plugin_method_add(p, n, m) do { n##_TYPE mtd = (m); __xmms_plugin_method_add(p, n, (xmms_plugin_method_t)mtd); } while(0)

void __xmms_plugin_method_add (xmms_plugin_t *plugin, const gchar *name,
			       xmms_plugin_method_t method);

void xmms_plugin_properties_add (xmms_plugin_t* const plugin, gint property);
void xmms_plugin_properties_remove (xmms_plugin_t* const plugin, gint property);
void xmms_plugin_info_add (xmms_plugin_t *plugin, gchar *key, gchar *value);

gboolean xmms_plugin_magic_add (xmms_plugin_t *plugin, const gchar *desc, const gchar *mime, ...);

/* config methods */
xmms_config_property_t *xmms_plugin_config_lookup (xmms_plugin_t *plugin, const gchar *value);
xmms_config_property_t *xmms_plugin_config_property_register (xmms_plugin_t *plugin, const gchar *value, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);

#endif /* __XMMS_PLUGIN_H__ */
