#include "playlist.h"

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
		xmms_playlist_entry_set_prop (e, XMMS_ENTRY_PROPERTY_ARTIST, apa);
		xmms_playlist_add (pl, e, XMMS_PLAYLIST_APPEND);
	}


	while ((entry = xmms_playlist_get_next (pl))) {
		printf ("Got %s from playlist\n", xmms_playlist_entry_get_uri (entry));
	}


	return 0;
	
}

