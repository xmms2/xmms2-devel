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




#ifndef __XMMS_DBUS_H__
#define __XMMS_DBUS_H__

#include "xmms/core.h"

gboolean xmms_dbus_init (xmms_core_t *core, const gchar *path);
void xmms_dbus_register_object (const gchar *objectpath, xmms_object_t *object);
void xmms_dbus_register_onchange (xmms_object_t *object, gchar *signal);

#endif
