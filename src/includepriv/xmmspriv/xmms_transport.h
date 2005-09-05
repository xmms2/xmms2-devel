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




#ifndef __XMMS_PRIV_TRANSPORT_H__
#define __XMMS_PRIV_TRANSPORT_H__

#include "xmms/xmms_transport.h"
#include "xmms/xmms_medialib.h"

#ifdef XMMS_OS_DARWIN 
# define XMMS_TRANSPORT_DEFAULT_BUFFERSIZE "131072"
#else
# define XMMS_TRANSPORT_DEFAULT_BUFFERSIZE "32768"
#endif

xmms_transport_t * xmms_transport_new ();
gboolean xmms_transport_open (xmms_transport_t *transport, 
			      xmms_medialib_entry_t entry);
void xmms_transport_start (xmms_transport_t *transport);
void xmms_transport_stop (xmms_transport_t *transport);
gboolean xmms_transport_plugin_open (xmms_transport_t *transport, 
				     xmms_medialib_entry_t entry, gpointer data);

void xmms_transport_buffering_start (xmms_transport_t *transport);

#endif
