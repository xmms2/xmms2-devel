/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#ifndef __XMMS_PRIV_CONVERTER_H__
#define __XMMS_PRIV_CONVERTER_H__

#include <xmmspriv/xmms_streamtype.h>
#include <xmms/xmms_sample.h>
#include <xmms/xmms_medialib.h>

typedef struct xmms_sample_converter_St xmms_sample_converter_t;
typedef guint (*xmms_sample_conv_func_t) (xmms_sample_converter_t *, xmms_sample_t *, guint , xmms_sample_t *);

xmms_sample_converter_t *xmms_sample_converter_init (xmms_stream_type_t *from, xmms_stream_type_t *to);

gint64 xmms_sample_convert_scale (xmms_sample_converter_t *conv, gint64 samples);
gint64 xmms_sample_convert_rev_scale (xmms_sample_converter_t *conv, gint64 samples);


/* internal? */
void xmms_sample_convert (xmms_sample_converter_t *conv, xmms_sample_t *in, guint len, xmms_sample_t **out, guint *outlen);
void xmms_sample_convert_reset (xmms_sample_converter_t *conv);
xmms_sample_converter_t *xmms_sample_audioformats_coerce (xmms_stream_type_t *in, const GList *goal_types);
xmms_stream_type_t *xmms_sample_converter_get_from (xmms_sample_converter_t *conv);
xmms_stream_type_t *xmms_sample_converter_get_to (xmms_sample_converter_t *conv);
void xmms_sample_converter_to_medialib (xmms_sample_converter_t *conv, xmms_medialib_entry_t entry);

#endif
