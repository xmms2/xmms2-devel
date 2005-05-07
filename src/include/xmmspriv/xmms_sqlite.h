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




#ifndef __XMMS_PRIV_SQLITE_H__
#define __XMMS_PRIV_SQLITE_H__

#include <sqlite3.h>

typedef int (*xmms_medialib_row_method_t) (void *pArg, int argc, char **argv, char **columnName);

sqlite3 *xmms_sqlite_open (guint *nextid, gboolean *c);
gboolean xmms_sqlite_query (sqlite3 *sql, xmms_medialib_row_method_t method, void *udata, char *query, ...);
void xmms_sqlite_close (sqlite3 *sql);


#endif
