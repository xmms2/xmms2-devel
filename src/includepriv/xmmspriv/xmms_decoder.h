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




#ifndef __XMMS_PRIV_DECODER_H__
#define __XMMS_PRIV_DECODER_H__

#include "xmms/xmms_decoder.h"
#include "xmms/xmms_transport.h"
#include "xmms/xmms_output.h"
#include "xmms/xmms_effect.h"
#include "xmms/xmms_error.h"


#define XMMS_DECODER_DEFAULT_BUFFERSIZE "131072"


xmms_decoder_t * xmms_decoder_new ();
gboolean xmms_decoder_open (xmms_decoder_t *decoder,
			    xmms_transport_t *transport);
gboolean xmms_decoder_init (xmms_decoder_t *decoder, GList *output_format_list, GList *effects);
xmms_audio_format_t *xmms_decoder_audio_format_to_get (xmms_decoder_t *decoder);
void xmms_decoder_start (xmms_decoder_t *decoder);
gint32 xmms_decoder_seek_ms (xmms_decoder_t *decoder, guint milliseconds, xmms_error_t *err);
gint32 xmms_decoder_seek_samples (xmms_decoder_t *decoder, guint samples, xmms_error_t *err);

guint xmms_decoder_read (xmms_decoder_t *decoder, gchar *buf, guint len);
gboolean xmms_decoder_iseos (xmms_decoder_t *decoder);

void xmms_decoder_stop (xmms_decoder_t *decoder);

void xmms_decoder_mediainfo_get (xmms_decoder_t *decoder, xmms_transport_t *transport);

#endif
