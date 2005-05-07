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




#ifndef __XMMS_LOG_H__
#define __XMMS_LOG_H__

#include <glib.h>

/* FIXME */
#define xmms_log_error xmms_log

#ifdef __GNUC__
void xmms_log_debug (const gchar *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void xmms_log (const gchar *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void xmms_log_fatal (const gchar *fmt, ...) __attribute__ ((format (printf, 1, 2)));
#else
void xmms_log_debug (const gchar *fmt, ...);
void xmms_log (const gchar *fmt, ...);
void xmms_log_fatal (const gchar *fmt, ...);
#endif

#define XMMS_STRINGIFY_NOEXPAND(x) #x
#define XMMS_STRINGIFY(x) XMMS_STRINGIFY_NOEXPAND(x)

#define DEBUG

#ifdef DEBUG
#define XMMS_DBG(fmt, args...) xmms_log_debug (__FILE__ ":" XMMS_STRINGIFY(__LINE__) ": " fmt, ## args)
#else
#define XMMS_DBG(fmt,...)
#endif

#endif
