
/** @file
 *  Controls playlist
 */

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
		case XMMS_PLAYLIST_APPEND:
			g_slist_append (playlist->list, (gpointer) file);
			break;
		case XMMS_PLAYLIST_PREPEND:
			g_slist_prepend (playlist->list, (gpointer) file);
		case XMMS_PLAYLIST_INSERT:
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

/* playlist_entry_* */

xmms_playlist_entry_t *
xmms_playlist_entry_new (gchar *uri)
{
	xmms_playlist_entry_t *ret;

	ret = g_new0 (xmms_playlist_entry_t, 1);
	ret->uri = g_strdup (uri);
	ret->properties = g_hash_table_new (g_str_hash, g_str_equal);

	return ret;
}

static void
xmms_playlist_entry_clone_foreach (gpointer key, gpointer value, gpointer udata)
{
	xmms_playlist_entry_t *new = (xmms_playlist_entry_t *) udata;

	xmms_playlist_entry_set_prop (new, g_strdup ((gchar *)key), g_strdup ((gchar *)value));
}

/** Make a copy of all properties in entry to new
  */

void
xmms_playlist_entry_copy_property (xmms_playlist_entry_t *entry, xmms_playlist_entry_t *newentry)
{
	g_return_if_fail (entry);
	g_return_if_fail (newentry);

	g_hash_table_foreach (entry->properties, xmms_playlist_entry_clone_foreach, (gpointer) newentry);

}

/**
 * Associates a value/key pair with a entry.
 * 
 * It copies the value/key and you'll need to call #xmms_playlist_entry_free
 * to free all strings.
 */

void
xmms_playlist_entry_set_prop (xmms_playlist_entry_t *entry, gchar *key, gchar *value)
{
	g_return_if_fail (entry);
	g_return_if_fail (key);
	g_return_if_fail (value);

	g_hash_table_insert (entry->properties, g_strdup (key), g_strdup (value));
	
}

void
xmms_playlist_entry_print (xmms_playlist_entry_t *entry)
{

	g_return_if_fail (entry);

	fprintf (stdout, "--------\n");
	fprintf (stdout, "Artist=%s\nAlbum=%s\nTitle=%s\nYear=%s\nTracknr=%s\nBitrate=%s\nComment=%s\nDuration=%ss\n", 
		xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_ARTIST),
		xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_ALBUM),
		xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_TITLE),
		xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_YEAR),
		xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_TRACKNR),
		xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_BITRATE),
		xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_COMMENT),
		xmms_playlist_entry_get_prop (entry, XMMS_ENTRY_PROPERTY_DURATION));
	fprintf (stdout, "--------\n");
}

void
xmms_playlist_entry_set_uri (xmms_playlist_entry_t *entry, gchar *uri)
{
	g_return_if_fail (entry);
	g_return_if_fail (uri);

	entry->uri = g_strdup (uri);
}

gchar *
xmms_playlist_entry_get_uri (xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (entry, NULL);

	return entry->uri;
}

gchar *
xmms_playlist_entry_get_prop (xmms_playlist_entry_t *entry, gchar *key)
{
	g_return_val_if_fail (entry, NULL);
	g_return_val_if_fail (key, NULL);

	return g_hash_table_lookup (entry->properties, key);
}

gint
xmms_playlist_entry_get_prop_int (xmms_playlist_entry_t *entry, gchar *key)
{
	gchar *t;
	g_return_val_if_fail (entry, -1);
	g_return_val_if_fail (key, -1);

	t = g_hash_table_lookup (entry->properties, key);

	if (t)
		return strtol (t, NULL, 10);
	else
		return 0;
}


static gboolean
xmms_playlist_entry_foreach_free (gpointer key, gpointer value, gpointer udata)
{
	g_free (key);
	g_free (value);
	return TRUE;
}

void
xmms_playlist_entry_free (xmms_playlist_entry_t *entry)
{
	g_free (entry->uri);
	g_hash_table_foreach_remove (entry->properties, xmms_playlist_entry_foreach_free, NULL);
	g_hash_table_destroy (entry->properties);
	g_free (entry);
}

static gchar *wellknowns[] = {
	XMMS_ENTRY_PROPERTY_ARTIST,
	XMMS_ENTRY_PROPERTY_ALBUM,
	XMMS_ENTRY_PROPERTY_TITLE,
	XMMS_ENTRY_PROPERTY_YEAR,
	XMMS_ENTRY_PROPERTY_TRACKNR,
	XMMS_ENTRY_PROPERTY_GENRE,
	XMMS_ENTRY_PROPERTY_BITRATE,
	XMMS_ENTRY_PROPERTY_COMMENT,
	XMMS_ENTRY_PROPERTY_DURATION,
	NULL 
};

gboolean
xmms_playlist_entry_is_wellknown (gchar *property)
{
	gint i = 0;

	while (wellknowns[i]) {
		if (g_strcasecmp (wellknowns[i], property) == 0)
			return TRUE;

		i++;
	}

	return FALSE;
}

