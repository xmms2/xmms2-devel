#ifndef __PLAYLIST_H__
#define __PLAYLIST_H__

#include <glib.h>

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

#endif
