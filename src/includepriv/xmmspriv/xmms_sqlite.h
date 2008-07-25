/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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




#ifndef __XMMS_PRIV_SQLITE_H__
#define __XMMS_PRIV_SQLITE_H__

#include <sqlite3.h>
#include <xmms/xmms_object.h>

typedef gboolean (*xmms_medialib_row_array_method_t) (xmmsv_t **row, gpointer udata);
typedef gboolean (*xmms_medialib_row_table_method_t) (GTree *row, gpointer udata);

sqlite3 *xmms_sqlite_open (void);
gboolean xmms_sqlite_create (gboolean *create);
gboolean xmms_sqlite_query_array (sqlite3 *sql, xmms_medialib_row_array_method_t method, gpointer udata, const gchar *query, ...);
gboolean xmms_sqlite_query_table (sqlite3 *sql, xmms_medialib_row_table_method_t method, gpointer udata, xmms_error_t *error, const gchar *query, ...);
gboolean xmms_sqlite_exec (sqlite3 *sql, const char *query, ...);
void xmms_sqlite_close (sqlite3 *sql);
void xmms_sqlite_print_version (void);
gchar *sqlite_prepare_string (const gchar *input);

#endif
