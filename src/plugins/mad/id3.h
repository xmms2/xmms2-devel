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




#ifndef __XMMS_MAD_ID3_H__
#define __XMMS_MAD_ID3_H__

typedef struct xmms_id3v2_header_St {
	guint8 ver;
	guint8 rev;
	guint32 flags;
	gint32 len;
} xmms_id3v2_header_t;

gboolean xmms_mad_id3v2_header (gchar *, xmms_id3v2_header_t *);
gboolean xmms_mad_id3v2_parse (gchar *, xmms_id3v2_header_t *, xmms_playlist_entry_t *);

gboolean xmms_mad_id3_parse (gchar *, xmms_playlist_entry_t *);

#endif
