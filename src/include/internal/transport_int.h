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




#ifndef __XMMS_TRANSPORT_INT_H__
#define __XMMS_TRANSPORT_INT_H__

/*
 * Macros
 */

#include "xmms/transport.h"
#include "xmms/playlist.h"

/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_transport_t * xmms_transport_new ();
gboolean xmms_transport_open (xmms_transport_t *transport, 
			      xmms_playlist_entry_t *entry);
const gchar *xmms_transport_mimetype_get (xmms_transport_t *transport);
const gchar *xmms_transport_mimetype_get_wait (xmms_transport_t *transport);
void xmms_transport_start (xmms_transport_t *transport);
GList *xmms_transport_stats (xmms_transport_t *transport, GList *list);

/*
 * Private defines -- do NOT rely on these in plugins
 */
#define XMMS_TRANSPORT_RINGBUF_SIZE 32768

#endif
