/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

#ifndef __XMMS_BINDATA_H__
#define __XMMS_BINDATA_H__

#include <xmmsc/xmmsc_compiler.h>

#include <glib.h>

G_BEGIN_DECLS

gchar *xmms_bindata_calculate_md5 (const guchar *data, gsize size, gchar ret[33]) XMMS_PUBLIC;
gboolean xmms_bindata_plugin_add (const guchar *data, gsize size, gchar hash[33]) XMMS_PUBLIC;

G_END_DECLS

#endif
