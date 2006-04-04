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




#ifndef __XMMS_PLAYLIST_PLUGIN_H__
#define __XMMS_PLAYLIST_PLUGIN_H__

#include "xmms/xmms_xformplugin.h"

typedef gboolean (*xmms_playlist_plugin_read_method_t) (xmms_xform_t *transport, guint playlist_id);
typedef gboolean (*xmms_playlist_plugin_can_handle_method_t) (const gchar *mimetype);
typedef GString *(*xmms_playlist_plugin_write_method_t) (guint32 *list);

#define XMMS_PLUGIN_METHOD_CAN_HANDLE_TYPE xmms_playlist_plugin_can_handle_method_t
#define XMMS_PLUGIN_METHOD_READ_PLAYLIST_TYPE xmms_playlist_plugin_read_method_t
#define XMMS_PLUGIN_METHOD_WRITE_PLAYLIST_TYPE xmms_playlist_plugin_write_method_t

#endif
