
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "playlist.h"
#include "util.h"


/*
 * Public functions
 */

xmms_playlist_entry_t *
xmms_playlist_entry_new (gchar *uri)
{
	xmms_playlist_entry_t *ret;

	ret = g_new0 (xmms_playlist_entry_t, 1);

	strncpy (ret->uri, uri, XMMS_MAX_URI_LEN);

	return ret;
}

void
xmms_playlist_entry_free (xmms_playlist_entry_t *entry)
{
	g_free (entry);
}

guint
xmms_playlist_entries (xmms_playlist_t *playlist)
{
	return g_slist_length (playlist->list);
}

gboolean
xmms_playlist_add (xmms_playlist_t *playlist, xmms_playlist_entry_t *file, gint options)
{

	if (file->uri && strstr (file->uri, "britney")) {
		XMMS_DBG ("Popular music detected: consider playing better music");
		exit (1);
	}
	
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

	n = g_slist_nth (playlist->list, 1);

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

