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


#include "xmms/plugin.h"
#include "xmms/playlist.h"
#include "xmms/output.h"
#include <glib.h>

/*
 * Method defintions.
 */ 

/** Callback to the query function. */
typedef int (*xmms_medialib_row_method_t) (void *pArg, int argc, char **argv, char **columnName);

typedef struct xmms_medialib_St xmms_medialib_t;

/*
 * Public functions
 */
gboolean xmms_medialib_init ();
gboolean xmms_medialib_entry_get (xmms_playlist_entry_t *entry);
gboolean xmms_medialib_entry_store (xmms_playlist_entry_t *entry);
void xmms_medialib_shutdown ();

void xmms_medialib_id_set ();
void xmms_medialib_playlist_set (xmms_playlist_t *p);

void xmms_medialib_output_register (xmms_output_t *output);
GList *xmms_medialib_select (gchar *query, xmms_error_t *error);
GList * xmms_medialib_select_entries (gchar *query, xmms_error_t *error);


#ifdef HAVE_SQLITE
#include <sqlite.h>

void xmms_medialib_sql_set (sqlite *sql);
sqlite *xmms_medialib_sql_get ();
gboolean xmms_sqlite_open ();
gboolean xmms_sqlite_query (xmms_medialib_row_method_t method, void *udata, char *query, ...);
void xmms_sqlite_close ();
#endif


#endif /* __XMMS_MEDIALIB_H__ */
