/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */




#ifndef __XMMS_MAD_ID3_H__
#define __XMMS_MAD_ID3_H__

typedef struct xmms_id3v2_header_St {
	guint8 ver;
	guint8 rev;
	guint32 flags;
	guint32 len;
} xmms_id3v2_header_t;

gboolean xmms_mad_id3v2_header (guchar *, xmms_id3v2_header_t *);
gboolean xmms_mad_id3v2_parse (guchar *, xmms_id3v2_header_t *, xmms_playlist_entry_t *);

gboolean xmms_mad_id3_parse (guchar *, xmms_playlist_entry_t *);

#endif
