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




#ifndef __XMMS_DECODER_H__
#define __XMMS_DECODER_H__


typedef struct xmms_decoder_St xmms_decoder_t;

#include "xmms/xmms_transport.h"
#include "xmms/xmms_medialib.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_plugin.h"

#define XMMS_DECODER_INIT_MEDIAINFO (1 << 0)
#define XMMS_DECODER_INIT_DECODING (1 << 1)

gpointer xmms_decoder_private_data_get (xmms_decoder_t *decoder);
void xmms_decoder_private_data_set (xmms_decoder_t *decoder, gpointer data);

xmms_transport_t *xmms_decoder_transport_get (xmms_decoder_t *decoder);
xmms_plugin_t *xmms_decoder_plugin_get (xmms_decoder_t *);
void xmms_decoder_write (xmms_decoder_t *decoder, gchar *buf, guint len);

void xmms_decoder_format_add (xmms_decoder_t *decoder, xmms_sample_format_t fmt, guint channels, guint rate);
xmms_audio_format_t *xmms_decoder_format_finish (xmms_decoder_t *decoder);


xmms_medialib_entry_t xmms_decoder_medialib_entry_get (xmms_decoder_t *decoder);
#endif /* __XMMS_DECODER_H__ */
