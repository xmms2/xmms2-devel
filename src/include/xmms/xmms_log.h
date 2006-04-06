/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 Peter Alm, Tobias Rundström, Anders Gustafsson
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


#define xmms_log_fatal g_error
#define xmms_log_error g_warning
#define xmms_log_info g_message
#define xmms_log_debug g_debug

#define XMMS_STRINGIFY_NOEXPAND(x) #x
#define XMMS_STRINGIFY(x) XMMS_STRINGIFY_NOEXPAND(x)

#define DEBUG

#ifndef _MSC_VER
#ifdef DEBUG
#define XMMS_DBG(fmt, ...) xmms_log_debug (__FILE__ ":" XMMS_STRINGIFY(__LINE__) ": " fmt, ## __VA_ARGS__)
#else
#define XMMS_DBG(fmt, ...)
#endif
#endif

#endif


