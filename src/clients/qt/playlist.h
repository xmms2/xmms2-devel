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




#ifndef __PLAYLIST_H__
#define __PLAYLIST_H__

#include <glib.h>
#include "xmmsclient.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct playlist_St {
	GList *list;
	GHashTable *id_table;
	xmmsc_connection_t *conn;
} playlist_t;

typedef struct playlist_entry_St {
	guint id;
	gchar *url;
	GHashTable *properties;
	gpointer data; /* User data */
} playlist_entry_t;
	
playlist_t *playlist_init (xmmsc_connection_t *conn);
GList *playlist_list_get (playlist_t *pl);
playlist_entry_t *playlist_entry_get (playlist_t *pl, guint id);
gpointer playlist_get_id_data (playlist_t *pl, unsigned int id);
void playlist_set_id_data (playlist_t *pl, unsigned int id, gpointer data);

#ifdef __cplusplus
}
#endif

#endif
