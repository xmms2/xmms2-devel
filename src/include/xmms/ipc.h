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

#ifndef __XMMS_IPC_H__
#define __XMMS_IPC_H__

#include "xmms/object.h"
#include "xmms/ipc_msg.h"

typedef enum {
	XMMS_IPC_CLIENT_STATUS_NEW,
} xmms_ipc_client_status_t;


typedef struct xmms_ipc_St xmms_ipc_t;
typedef struct xmms_ipc_client_St xmms_ipc_client_t;

void xmms_ipc_object_register (xmms_ipc_objects_t objectid, xmms_object_t *object);
void xmms_ipc_object_unregister (xmms_ipc_objects_t objectid);
xmms_ipc_t * xmms_ipc_init (void);
gboolean xmms_ipc_setup_server (const gchar *path);
gboolean xmms_ipc_setup_with_gmain (xmms_ipc_t *ipc);
gboolean xmms_ipc_client_msg_write (xmms_ipc_client_t *client, xmms_ipc_msg_t *msg);
void xmms_ipc_broadcast_register (xmms_object_t *object, xmms_ipc_signals_t signalid);
void xmms_ipc_broadcast_unregister (xmms_object_t *object, xmms_ipc_signals_t signalid);
void xmms_ipc_signal_register (xmms_object_t *object, xmms_ipc_signals_t signalid);
void xmms_ipc_signal_unregister (xmms_object_t *object, xmms_ipc_signals_t signalid);
gboolean xmms_ipc_has_pending (guint signalid);

#endif

