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


/*
 * Type definitions
 */

/**
 * Structure describing decoder-objects.
 * Do not modify this structure directly, use the functions.
 */
struct xmms_decoder_St;
typedef struct xmms_decoder_St xmms_decoder_t;

#include "xmms/playlist.h"
#include "xmms/transport.h"
#include "xmms/output.h"

/*
 * Decoder plugin methods
 */

typedef gboolean (*xmms_decoder_can_handle_method_t) (const gchar *mimetype);
typedef gboolean (*xmms_decoder_init_method_t) (xmms_decoder_t *decoder);
typedef gboolean (*xmms_decoder_new_method_t) (xmms_decoder_t *decoder, const gchar *mimetype);
typedef gboolean (*xmms_decoder_destroy_method_t) (xmms_decoder_t *decoder);
typedef gboolean (*xmms_decoder_decode_block_method_t) (xmms_decoder_t *decoder);
typedef gboolean (*xmms_decoder_seek_method_t) (xmms_decoder_t *decoder, guint samples);
typedef void (*xmms_decoder_get_mediainfo_method_t) (xmms_decoder_t *decoder);

/*
 * Public function prototypes
 */

gpointer xmms_decoder_private_data_get (xmms_decoder_t *decoder);
void xmms_decoder_private_data_set (xmms_decoder_t *decoder, gpointer data);

xmms_transport_t *xmms_decoder_transport_get (xmms_decoder_t *decoder);
xmms_output_t *xmms_decoder_output_get (xmms_decoder_t *decoder);
xmms_plugin_t *xmms_decoder_plugin_get (xmms_decoder_t *);
void xmms_decoder_write (xmms_decoder_t *decoder, gchar *buf, guint len);
void xmms_decoder_samplerate_set (xmms_decoder_t *decoder, guint rate);
guint xmms_decoder_samplerate_get (xmms_decoder_t *decoder);
xmms_playlist_entry_t *xmms_decoder_mediainfo_get (xmms_decoder_t *decoder, 
			    		           xmms_transport_t *transport);
void xmms_decoder_entry_mediainfo_set (xmms_decoder_t *decoder, 
				       xmms_playlist_entry_t *entry);
xmms_decoder_t *xmms_decoder_new_stacked (xmms_output_t *output, 
					  xmms_transport_t *transport, 
					  xmms_playlist_entry_t *entry);

#endif /* __XMMS_DECODER_H__ */
