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




#include "xmms/playlist.h"

int main ()
{
	xmms_playlist_t *pl;
	xmms_playlist_entry_t *entry;
	GList *l;
	gint i;

	g_thread_init (NULL);

	pl = xmms_playlist_init ();
	
	xmms_playlist_shuffle (pl);

	for (i=0; i < 10 ; i++) {
		gchar *apa = g_strdup_printf ("%d", i);
		xmms_playlist_entry_t *e = xmms_playlist_entry_new (apa);
		xmms_playlist_entry_set_prop (e, XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST, apa);
		xmms_playlist_add (pl, e, XMMS_PLAYLIST_APPEND);
	}


	while ((entry = xmms_playlist_get_next (pl))) {
		printf ("Got %s from playlist\n", xmms_playlist_entry_get_uri (entry));
	}


	return 0;
	
}

