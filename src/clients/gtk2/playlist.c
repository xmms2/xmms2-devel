/**
 * @file This is a local playlist for be used in clients.
 * You can copy it too your own program.
 *
 * This is NOT threadsafe code. This should only be used in a
 * singlethreaded program.
 */

#include "xmmsclient.h"
#include "playlist.h"
#include "xmms/signal_xmms.h"
#include <glib.h>

#include <stdio.h>

//#define DBG(fmt, args...) printf (__FILE__ ": " fmt "\n", ## args)
#define DBG(fmt, args...)

static void
playlist_add (void *userdata, void *arg)
{
	playlist_t *pl = userdata;
	playlist_entry_t *entry;
	guint id;
	GList *new, *node;

	DBG ("playlist add");

	id = GPOINTER_TO_UINT (arg);
	entry = g_new0 (playlist_entry_t, 1);
	entry->id = id;
	entry->properties = g_hash_table_new (g_str_hash, g_str_equal);

	new = g_list_alloc ();
	new->data = (gpointer) entry;

	node = g_list_last (pl->list);
	if (!node) {
		pl->list = new;
	} else {
		node->next = new;
		new->prev = node;
	}

	g_hash_table_insert (pl->id_table, GUINT_TO_POINTER (id), (gpointer) new);
	
	/* Get the information about this entry */
	xmmsc_playlist_get_mediainfo (pl->conn, id);
}

static gboolean
foreach_free (gpointer key, gpointer value, gpointer udata)
{
	g_free (key);
	g_free (value);
	return TRUE;
}

static void
playlist_entry_free (playlist_entry_t *e)
{
	g_hash_table_foreach_remove (e->properties, foreach_free, NULL);
	g_free (e->url);
	g_free (e);
}

static void
playlist_remove (void *userdata, void *arg)
{
	playlist_t *pl = userdata;
	GList *node;
	guint id;
	
	DBG ("playlist remove");

	id = GPOINTER_TO_UINT (arg);
	node = g_hash_table_lookup (pl->id_table, GUINT_TO_POINTER (id));
	if (node) {
		playlist_entry_t *e = node->data;
		pl->list = g_list_remove_link (pl->list, node);
		g_hash_table_remove (pl->id_table, GUINT_TO_POINTER (id));

		/* Cleanup */
		playlist_entry_free (e);
		g_list_free_1 (node);
	}

}

static void
playlist_clear (void *userdata, void *arg)
{
	playlist_t *pl = userdata;
	GList *node;
	
	DBG ("playlist clear");

	for (node = pl->list; node; node = g_list_next (node)) {
		playlist_entry_t *e = node->data;
		playlist_entry_free (e);
	}
	
	g_list_free (pl->list);
	pl->list = NULL;
	g_hash_table_destroy (pl->id_table);
	pl->id_table = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
playlist_list (void *userdata, void *arg)
{
	guint32 *list = arg;
	guint i=0;

	DBG ("playlist list");

	playlist_clear (userdata, NULL);

	while (list[i]) {
		playlist_add (userdata, GUINT_TO_POINTER (list[i]));
		i++;
	}
}

static void
playlist_reread (void *userdata, void *arg)
{
	playlist_t *pl = userdata;

	DBG ("Playlist reread");

	xmmsc_playlist_list (pl->conn);
}


static void 
foreach_copy (gpointer key, gpointer value, gpointer udata)
{
	playlist_entry_t *entry = udata;
	gchar *_key = key;
	gchar *_value = value;

	if (g_strcasecmp (_key, "url") == 0)
		entry->url = g_strdup (value);
	else if (g_strcasecmp (_key, "id") == 0)
		return;
	else 
		g_hash_table_insert (entry->properties, 
				     g_strdup (_key), g_strdup (_value));
}

static void
playlist_mediainfo (void *userdata, void *arg)
{
	playlist_t *pl = userdata;
	GHashTable *table = (GHashTable *)arg;
	GList *n;
	playlist_entry_t *entry;
	guint id;

	DBG ("Playlist mediainfo");

	id = GPOINTER_TO_UINT (g_hash_table_lookup (table, "id"));
	
	n = g_hash_table_lookup (pl->id_table, GUINT_TO_POINTER (id));
	if (!n)
		return;
	
	entry = n->data;

	g_hash_table_foreach (table, foreach_copy, (gpointer) entry);

}

playlist_entry_t *
playlist_entry_get (playlist_t *pl, guint id)
{
	GList *n;

	n = g_hash_table_lookup (pl->id_table, GUINT_TO_POINTER (id));
	if (n)
		return n->data;

	return NULL;
}

GList *
playlist_list_get (playlist_t *pl)
{
	return pl->list;
}

playlist_t *
playlist_init (xmmsc_connection_t *conn)
{
	playlist_t *pl;

	pl = g_new0 (playlist_t, 1);
	pl->id_table = g_hash_table_new (g_direct_hash, g_direct_equal);
	pl->list = NULL;
	pl->conn = conn;

	/* Setup callbacks */
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_ADD, playlist_add, pl);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_REMOVE, playlist_remove, pl);
/*	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_MOVE, playlist_move, pl);*/
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_SHUFFLE, playlist_reread, pl);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_CLEAR, playlist_clear, pl);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_SORT, playlist_reread, pl);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_LIST, playlist_list, pl);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_MEDIAINFO, playlist_mediainfo, pl);

	return pl;
}

