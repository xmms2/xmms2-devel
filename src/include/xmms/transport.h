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




#ifndef __XMMS_TRANSPORT_H__
#define __XMMS_TRANSPORT_H__

#include <glib.h>
#include "xmms/plugin.h"
#include "xmms/playlist.h"

/*
 * Type definitions
 */

typedef enum {
	XMMS_TRANSPORT_ENTRY_FILE,
	XMMS_TRANSPORT_ENTRY_DIR
} xmms_transport_entry_type_t;

typedef struct xmms_transport_St xmms_transport_t;
typedef struct xmms_transport_entry_St xmms_transport_entry_t;

#define XMMS_TRANSPORT_SEEK_SET 0
#define XMMS_TRANSPORT_SEEK_END 1
#define XMMS_TRANSPORT_SEEK_CUR 2

/*
 * Transport plugin methods
 */

typedef gboolean (*xmms_transport_can_handle_method_t) (const gchar *uri);
typedef gboolean (*xmms_transport_open_method_t) (xmms_transport_t *transport,
						  const gchar *uri);
typedef void (*xmms_transport_close_method_t) (xmms_transport_t *transport);
typedef gint (*xmms_transport_read_method_t) (xmms_transport_t *transport,
					      gchar *buffer, guint length);
typedef gint (*xmms_transport_seek_method_t) (xmms_transport_t *transport, 
					      guint offset, gint whence);
typedef gint (*xmms_transport_size_method_t) (xmms_transport_t *transport);
typedef GList *(*xmms_transport_list_method_t) (const gchar *path);

/*
 * Public function prototypes
 */

void xmms_transport_ringbuf_resize (xmms_transport_t *transport, gint size);
GList *xmms_transport_list (const gchar *path);
void xmms_transport_list_free (GList *);
gpointer xmms_transport_plugin_data_get (xmms_transport_t *transport);
void xmms_transport_plugin_data_set (xmms_transport_t *transport, gpointer data);
void xmms_transport_mimetype_set (xmms_transport_t *transport, const gchar *mimetype);
gint xmms_transport_read (xmms_transport_t *transport, gchar *buffer, guint len);
void xmms_transport_wait (xmms_transport_t *transport);
gboolean xmms_transport_seek (xmms_transport_t *transport, gint offset, gint whence);
gint xmms_transport_size (xmms_transport_t *transport);
xmms_plugin_t *xmms_transport_plugin_get (const xmms_transport_t *transport);
const gchar *xmms_transport_url_get (const xmms_transport_t *const transport);
void xmms_transport_entry_mediainfo_set (xmms_transport_t *transport, xmms_playlist_entry_t *entry);
const gchar *xmms_transport_suburl_get (const xmms_transport_t *const transport);
void xmms_transport_close (xmms_transport_t *transport);
gboolean xmms_transport_can_seek (xmms_transport_t *transport);
gboolean xmms_transport_islocal (xmms_transport_t *transport);
gboolean xmms_transport_plugin_open (xmms_transport_t *transport, 
				     xmms_playlist_entry_t *entry, gpointer data);


xmms_transport_entry_t *xmms_transport_entry_new 
			(gchar *path, xmms_transport_entry_type_t type);
void xmms_transport_entry_free (xmms_transport_entry_t *entry);
xmms_transport_entry_type_t xmms_transport_entry_type_get
			    (xmms_transport_entry_t *entry);
const gchar *xmms_transport_entry_path_get (xmms_transport_entry_t *entry);

#endif /* __XMMS_TRANSPORT_H__ */
