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


#ifndef __CDAE_H__
#define __CDAE_H__

#include <glib.h>
#include "xmms/xmms_medialib.h"

#define XMMS_CDAE_FRAMESIZE 2352

#define LBA(msf) ((msf.minute * 60 + msf.second) * 75 + msf.frame)

typedef struct xmms_cdae_track_St {
	gint minute;
	gint second;
	gint frame;
	gboolean data_track;
} xmms_cdea_track_t;

typedef struct xmms_cdea_toc_St {
	gint first_track, last_track;
	xmms_cdea_track_t track[100];
	xmms_cdea_track_t leadout;
} xmms_cdae_toc_t;

typedef struct {
	gint track;
	gint fd;
	xmms_cdae_toc_t *toc;
	gchar *mime;
	gint pos;
	gint endpos;
} xmms_cdae_data_t;

/* OS specific calls: */
xmms_cdae_toc_t *xmms_cdae_get_toc (int fd);
gint xmms_cdae_read_data (gint fd, gint pos, gchar *buffer, gint len);

/** CDDB Meck */
xmms_medialib_entry_t xmms_cdae_cddb_query (xmms_cdae_toc_t *toc, gchar *server, gint track);

#endif
	
