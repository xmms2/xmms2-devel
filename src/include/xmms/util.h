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




#ifndef __XMMS_UTIL_H__

#include <stdio.h>

#include "xmms/log.h"

#define XMMS_PATH_MAX 255

#define XMMS_STRINGIFY_NOEXPAND(x) #x
#define XMMS_STRINGIFY(x) XMMS_STRINGIFY_NOEXPAND(x)

#define DEBUG

#ifdef DEBUG
#define XMMS_DBG(fmt, args...) xmms_log_debug (__FILE__ ": " fmt, ## args)
#else
#define XMMS_DBG(fmt,...)
#endif

#define XMMS_MTX_LOCK(a)  g_mutex_lock (a)
#define XMMS_MTX_UNLOCK(a) g_mutex_unlock (a)

gchar *xmms_util_decode_path (const gchar *path);
gchar *xmms_util_encode_path (gchar *path);
guint xmms_util_time (void);

#endif
