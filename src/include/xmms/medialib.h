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




#ifndef __XMMS_MEDIALIB_H__
#define __XMMS_MEDIALIB_H__

typedef struct xmms_medialib_St xmms_medialib_t;

#include "xmms/plugin.h"
#include "xmms/playlist.h"
#include <glib.h>

/*
 * Method defintions.
 */ 

/** Callback to the query function. */
typedef int (*xmms_medialib_row_method_t) (void *pArg, int argc, char **argv, char **columnName);

/*
 * Public functions
 */
xmms_medialib_t *xmms_medialib_init (void);
gboolean xmms_medialib_entry_store (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry);
void xmms_medialib_close (xmms_medialib_t *medialib);

void xmms_medialib_id_set (xmms_medialib_t *medialib, guint id);
void xmms_medialib_log_entry_stop (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry, guint value);
void xmms_medialib_log_entry_start (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry);

#ifdef HAVE_SQLITE
#include <sqlite.h>

void xmms_medialib_sql_set (xmms_medialib_t *medialib, sqlite *sql);
sqlite *xmms_medialib_sql_get (xmms_medialib_t *medialib);
gboolean xmms_sqlite_open (xmms_medialib_t *medialib);
gboolean xmms_sqlite_query (xmms_medialib_t *medialib, xmms_medialib_row_method_t method, void *udata, char *query, ...);
void xmms_sqlite_close (xmms_medialib_t *medialib);
#endif


#endif /* __XMMS_MEDIALIB_H__ */
