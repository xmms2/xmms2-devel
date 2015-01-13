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




#ifndef __XMMS_STREAMTYPE_H__
#define __XMMS_STREAMTYPE_H__

#include <xmmsc/xmmsc_compiler.h>

#include <glib.h>
#include <string.h>

G_BEGIN_DECLS

#define XMMS_STREAM_TYPE_BEGIN "placeholder"

typedef enum xmms_stream_type_key_E {
	XMMS_STREAM_TYPE_END,
	XMMS_STREAM_TYPE_MIMETYPE,
	XMMS_STREAM_TYPE_URL,
	XMMS_STREAM_TYPE_FMT_FORMAT,
	XMMS_STREAM_TYPE_FMT_CHANNELS,
	XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	XMMS_STREAM_TYPE_PRIORITY,
	XMMS_STREAM_TYPE_NAME
} xmms_stream_type_key_t;

struct xmms_stream_type_St;
typedef struct xmms_stream_type_St xmms_stream_type_t;

#define XMMS_STREAM_TYPE_PRIORITY_FALLBACK 40
#define XMMS_STREAM_TYPE_PRIORITY_DEFAULT 50

const char *xmms_stream_type_get_str (const xmms_stream_type_t *st, xmms_stream_type_key_t key) XMMS_PUBLIC;
gint xmms_stream_type_get_int (const xmms_stream_type_t *st, xmms_stream_type_key_t key) XMMS_PUBLIC;

G_END_DECLS

#endif
