
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>

#include "playlist.h"


/*
 * Public functions
 */

gboolean
xmms_playlist_add (xmms_playlist_t *playlist, xmms_playlist_entry_t *file, gint options)
{
	switch (options) {
		case XMMS_PL_APPEND:
			g_slist_append (playlist->list, (gpointer) file);
			break;
		case XMMS_PL_PREPEND:
			g_slist_prepend (playlist->list, (gpointer) file);
		case XMMS_PL_INSERT:
			break;
	}

	return TRUE;

}

xmms_playlist_entry_t *
xmms_playlist_pop (xmms_playlist_t *playlist)
{
	xmms_playlist_entry_t *r=NULL;
	GSList *n;

	n = g_slist_nth (playlist->list, 0);

	if (n) {
		r = n->data;
		g_slist_remove (playlist->list, r);
	}
	
	return r;

}

xmms_playlist_t *
xmms_playlist_init ()
{
	xmms_playlist_t *ret;

	ret = g_new0 (xmms_playlist_t, 1);
	ret->list = g_slist_alloc ();

	return ret;
}

void
xmms_playlist_close (xmms_playlist_t *playlist)
{
}

