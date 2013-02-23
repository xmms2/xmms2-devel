/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 */




#ifndef __XMMS_ID3V2_H__
#define __XMMS_ID3V2_H__

#include <xmms/xmms_medialib.h>

typedef struct xmms_id3v2_header_St {
	guint8 ver;
	guint8 rev;
	guint32 flags;
	guint32 len;
} xmms_id3v2_header_t;

gboolean xmms_id3v2_is_header (guchar *, xmms_id3v2_header_t *);
gboolean xmms_id3v2_parse (xmms_xform_t *xform, guchar *buf, xmms_id3v2_header_t *head);

#endif
