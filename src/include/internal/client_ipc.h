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


#ifndef __XMMSC_IPC_H__
#define __XMMSC_IPC_H__

#include "xmms/ipc_msg.h"
#include "xmms/xmmsclient.h"

typedef struct xmmsc_ipc_St xmmsc_ipc_t;

typedef void (*xmmsc_ipc_wakeup_t) (xmmsc_ipc_t *);

xmmsc_ipc_t *xmmsc_ipc_init (void);
gboolean xmmsc_ipc_io_in_callback (xmmsc_ipc_t *ipc);
gboolean xmmsc_ipc_msg_write (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg, guint32 cid);
void xmmsc_ipc_disconnect (xmmsc_ipc_t *ipc);
gpointer xmmsc_ipc_private_data_get (xmmsc_ipc_t *ipc);
void xmmsc_ipc_private_data_set (xmmsc_ipc_t *ipc, gpointer data);
void xmmsc_ipc_destroy (xmmsc_ipc_t *ipc);
gboolean xmmsc_ipc_connect (xmmsc_ipc_t *ipc, gchar *path);
void xmmsc_ipc_error_set (xmmsc_ipc_t *ipc, gchar *error);
gint xmmsc_ipc_fd_get (xmmsc_ipc_t *ipc);

void xmmsc_ipc_result_register (xmmsc_ipc_t *ipc, xmmsc_result_t *res);
xmmsc_result_t *xmmsc_ipc_result_lookup (xmmsc_ipc_t *ipc, guint cid);
void xmmsc_ipc_result_unregister (xmmsc_ipc_t *ipc, xmmsc_result_t *res);
void xmmsc_ipc_wait_for_event (xmmsc_ipc_t *ipc, guint timeout);

enum {
	XMMSC_IPC_IO_IN,
	XMMSC_IPC_IO_OUT,
};

#endif

