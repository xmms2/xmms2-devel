/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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

#include <glib.h>
#include "xmms/xmms_object.h"

G_BEGIN_DECLS

void xmms_ipc_object_register (xmms_ipc_objects_t objectid, xmms_object_t *object);
void xmms_ipc_object_unregister (xmms_ipc_objects_t objectid);
void xmms_ipc_broadcast_register (xmms_object_t *object, xmms_ipc_signals_t signalid);
void xmms_ipc_broadcast_unregister (xmms_ipc_signals_t signalid);
void xmms_ipc_signal_register (xmms_object_t *object, xmms_ipc_signals_t signalid);
void xmms_ipc_signal_unregister (xmms_ipc_signals_t signalid);

G_END_DECLS

#endif

