#include "playlist.h"

int main ()
{
	xmms_playlist_t *pl;
	xmms_playlist_entry_t *entry;
	GList *l;
	gint i;

	g_thread_init (NULL);

	pl = xmms_playlist_init ();

	for (i=0; i < 10 ; i++) {
		gchar *apa = g_strdup_printf ("%d", i);
		xmms_playlist_entry_t *e = xmms_playlist_entry_new (apa);
		xmms_playlist_entry_set_prop (e, XMMS_ENTRY_PROPERTY_ARTIST, apa);
		xmms_playlist_add (pl, e, XMMS_PLAYLIST_APPEND);
	}


	/*
	l = xmms_playlist_list (pl);
	while (l) {
		printf ("%s ", xmms_playlist_entry_get_prop (l->data, XMMS_ENTRY_PROPERTY_ARTIST));
		l = g_list_next (l);
	}*/

/*	printf ("Sorterar\n");
	xmms_playlist_sort (pl, XMMS_ENTRY_PROPERTY_ARTIST);*/

/*	entry = xmms_playlist_get_byid (pl, 300);

	printf ("aupa = %s", xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_ARTIST));*/

	xmms_playlist_shuffle (pl);

	l = xmms_playlist_list (pl);
	while (l) {
		printf ("%s - %d\n", xmms_playlist_entry_get_prop (l->data, XMMS_ENTRY_PROPERTY_ARTIST), xmms_playlist_entry_id_get (l->data));
		l = g_list_next (l);
	}

	return 0;
	
}

