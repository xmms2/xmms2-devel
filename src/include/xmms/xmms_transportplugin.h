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




#ifndef __XMMS_TRANSPORTPLUGIN_H__
#define __XMMS_TRANSPORTPLUGIN_H__

#include <glib.h>

/*
 * Type definitions
 */


#include "xmms/xmms_error.h"
#include "xmms/xmms_plugin.h"
#include "xmms/xmms_medialib.h"
#include "xmms/xmms_transport.h"

/*
 * Transport plugin methods
 */

typedef gboolean (*xmms_transport_can_handle_method_t) (const gchar *uri);
typedef guint (*xmms_transport_lmod_method_t) (xmms_transport_t *transport);
typedef gboolean (*xmms_transport_open_method_t) (xmms_transport_t *transport,
						  const gchar *uri);
typedef void (*xmms_transport_close_method_t) (xmms_transport_t *transport);
typedef gint (*xmms_transport_read_method_t) (xmms_transport_t *transport,
					      gchar *buffer, guint length, xmms_error_t *error);
typedef gint (*xmms_transport_seek_method_t) (xmms_transport_t *transport, 
					      gint offset, gint whence);
typedef gint (*xmms_transport_size_method_t) (xmms_transport_t *transport);
typedef GList *(*xmms_transport_list_method_t) (const gchar *path);

/*
 * Public function prototypes
 */

void xmms_transport_ringbuf_resize (xmms_transport_t *transport, gint size);
gpointer xmms_transport_private_data_get (xmms_transport_t *transport);
void xmms_transport_private_data_set (xmms_transport_t *transport, gpointer data);
void xmms_transport_mimetype_set (xmms_transport_t *transport, const gchar *mimetype);
xmms_plugin_t *xmms_transport_plugin_get (const xmms_transport_t *transport);
xmms_medialib_entry_t xmms_transport_medialib_entry_get (const xmms_transport_t *const transport);

#endif
