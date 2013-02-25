/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#ifndef __XMMS_PRIV_STREAMTYPE_H__
#define __XMMS_PRIV_STREAMTYPE_H__

#include <stdarg.h>
#include <xmms/xmms_streamtype.h>

xmms_stream_type_t *xmms_stream_type_parse (va_list ap);
gboolean xmms_stream_type_match (const xmms_stream_type_t *in_type, const xmms_stream_type_t *out_type);
xmms_stream_type_t *xmms_stream_type_coerce (const xmms_stream_type_t *in, const GList *goal_types);
xmms_stream_type_t *_xmms_stream_type_new (const gchar *begin, ...);


#endif
