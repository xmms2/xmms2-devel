
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
#include "signal_xmms.h"

static void xmms_playlist_entry_free (xmms_playlist_entry_t *entry);

/*
 * Public functions
 */

#define XMMS_PLAYLIST_LOCK(a) g_mutex_lock (a->mutex)
#define XMMS_PLAYLIST_UNLOCK(a) g_mutex_unlock (a->mutex)


/** Waits for something to happen in playlist
 * 
 *  This function will block until someone adds or reposition something
 *  in the playlist 
 */

void
xmms_playlist_wait (xmms_playlist_t *playlist)
{
	g_return_if_fail (playlist);

	XMMS_DBG ("Waiting for playlist ...");
	
	playlist->is_waiting = TRUE;
	g_cond_wait (playlist->cond, playlist->mutex);
	playlist->is_waiting = FALSE;

}

/** Total number of entries in the playlist. */

guint
xmms_playlist_entries_total (xmms_playlist_t *playlist)
{
	guint ret;
	ret = g_hash_table_size (playlist->id_table);
	return ret;
}

/** Number of entries *after* the nextsong pointer. */

guint
xmms_playlist_entries_left (xmms_playlist_t *playlist)
{
	guint ret;
	ret = g_list_length (playlist->currententry);
	return ret;
}

/** shuffles playlist */

void
xmms_playlist_shuffle (xmms_playlist_t *playlist)
{
        gint len;
        gint i, j;
        GList *node, **ptrs;
	xmms_playlist_changed_msg_t *chmsg;

	g_return_if_fail (playlist);

	XMMS_PLAYLIST_LOCK (playlist);

	len = xmms_playlist_entries_total (playlist);
                                                                                  
        if (!len) {
		XMMS_PLAYLIST_UNLOCK (playlist);
                return;
	}
                                                                                  
        ptrs = g_new (GList *, len);
                                                                                  
        for (node = playlist->list, i = 0; i < len; node = g_list_next (node), i++)
                ptrs[i] = node;
                                                                                  
        j = random() % len;
        playlist->list = ptrs[j];
        ptrs[j]->next = NULL;
        ptrs[j] = ptrs[0];

        for (i = 1; i < len; i++)
        {
                j = random() % (len - i);
                playlist->list->prev = ptrs[i + j];
                ptrs[i + j]->next = playlist->list;
                playlist->list = ptrs[i + j];
                ptrs[i + j] = ptrs[i];
        }
        playlist->list->prev = NULL;
                                                                                  
        g_free(ptrs);

	chmsg = g_new0 (xmms_playlist_changed_msg_t, 1);
	chmsg->type = XMMS_PLAYLIST_CHANGED_SHUFFLE;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, chmsg);
	g_cond_signal (playlist->cond);

	XMMS_PLAYLIST_UNLOCK (playlist);
}

/** removes entry from playlist */

gboolean 
xmms_playlist_id_remove (xmms_playlist_t *playlist, guint id)
{
	GList *node;
	xmms_playlist_changed_msg_t *chmsg;

	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (id, FALSE);

	XMMS_PLAYLIST_LOCK (playlist);
	node = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER (id));
	if (!node) {
		XMMS_PLAYLIST_UNLOCK (playlist);
		return FALSE;
	}
	if (node == playlist->currententry) {
		playlist->currententry = g_list_next (node);
	}
	g_hash_table_remove (playlist->id_table, GUINT_TO_POINTER (id));
	xmms_playlist_entry_unref (node->data);
	playlist->list = g_list_remove_link (playlist->list, node);
	g_list_free_1 (node);

	chmsg = g_new0 (xmms_playlist_changed_msg_t, 1);
	chmsg->type = XMMS_PLAYLIST_CHANGED_REMOVE;
	chmsg->id = id;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, chmsg);
	g_cond_signal (playlist->cond);
	
	XMMS_PLAYLIST_UNLOCK (playlist);

	return TRUE;
}

/** move entry in playlist */

gboolean
xmms_playlist_id_move (xmms_playlist_t *playlist, guint id, gint steps)
{
	xmms_playlist_entry_t *entry;
	xmms_playlist_changed_msg_t *chmsg;
	GList *node;
	gint i;

	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (id, FALSE);
	g_return_val_if_fail (steps != 0, FALSE);

	XMMS_DBG ("Moving %d, %d steps", id, steps);

	XMMS_PLAYLIST_LOCK (playlist);
	
	node = g_hash_table_lookup (playlist->id_table, GINT_TO_POINTER (id));
	
	if (!node) {
		XMMS_PLAYLIST_UNLOCK (playlist);
		return FALSE;
	}

	entry = node->data;

	if (steps > 0) {
		GList *next=node->next;

		playlist->list = g_list_remove_link (playlist->list, node);

		for (i=0; next && (i < steps); i++) {
			next = g_list_next (next);
		}

		if (next) {
			playlist->list = g_list_insert_before (playlist->list, next, node->data);
		} else {
			playlist->list = g_list_append (playlist->list, node->data);
		}

		g_list_free_1 (node);
	} else {
		GList *prev=node->prev;

		playlist->list = g_list_remove_link (playlist->list, node);

		for (i=0; prev && (i < abs (steps) - 1); i++) {
			prev = g_list_previous (prev);
		}

		if (prev) {
			playlist->list= g_list_insert_before (playlist->list, prev, node->data);
		} else {
			playlist->list = g_list_prepend (playlist->list, node->data);
		}
		g_list_free_1 (node);

	}

	chmsg = g_new0 (xmms_playlist_changed_msg_t, 1);
	chmsg->type = XMMS_PLAYLIST_CHANGED_MOVE;
	chmsg->id = id;
	chmsg->arg = GINT_TO_POINTER (steps);
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, chmsg);

	g_cond_signal (playlist->cond);

	XMMS_PLAYLIST_UNLOCK (playlist);

	return TRUE;

}

/** Adds a xmms_playlist_entry_t to the playlist.
 *
 *  This will append or prepend the entry according to
 *  the option.
 *  This function will wake xmms_playlist_wait.
 *  @param options should be XMMS_PLAYLIST_APPEND or XMMS_PLAYLIST_PREPEND
 */

gboolean
xmms_playlist_add (xmms_playlist_t *playlist, xmms_playlist_entry_t *file, gint options)
{
	GList *new, *node;
	xmms_playlist_changed_msg_t *chmsg;

	if (file->uri && strstr (file->uri, "britney")) {
		XMMS_DBG ("Popular music detected: consider playing better music");
		exit (1);
	}

	g_return_val_if_fail (!file->id, FALSE);

	new = g_list_alloc ();
	new->data = (gpointer) file;

	XMMS_PLAYLIST_LOCK (playlist);

	file->id = playlist->nextid++;
	xmms_playlist_entry_ref (file); /* reference this entry */
	
	switch (options) {
		case XMMS_PLAYLIST_APPEND:
			node = g_list_last (playlist->list);
			if (!node) {
				playlist->list = new;
			} else {
				node->next = new;
				new->prev = node;
			}
			break;
		case XMMS_PLAYLIST_PREPEND:
			if (!playlist->list) {
				playlist->list = new;
			} else {
				new->next = playlist->list;
				new->prev = NULL;
				playlist->list = new;
			}
			break;
		case XMMS_PLAYLIST_INSERT:
			break;
	}
	
	g_hash_table_insert (playlist->id_table, GUINT_TO_POINTER (file->id), new);

	chmsg = g_new0 (xmms_playlist_changed_msg_t, 1);
	chmsg->type = XMMS_PLAYLIST_CHANGED_ADD;
	chmsg->id = file->id;
	chmsg->arg = GINT_TO_POINTER (options);
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, chmsg);

	g_cond_signal (playlist->cond);

	XMMS_DBG ("Added with id %d - %s", file->id, file->uri);

	XMMS_PLAYLIST_UNLOCK (playlist);

	return TRUE;

}

/** Get a entry based on position in playlist.
 * 
 *  @returns a xmms_playlist_entry_t that lives on the given position.
 *  @param pos is the position in the playlist where 0 is the first.
 */

xmms_playlist_entry_t *
xmms_playlist_get_byid (xmms_playlist_t *playlist, guint id)
{
	GList *r = NULL;

	g_return_val_if_fail (playlist, NULL);

	XMMS_PLAYLIST_LOCK (playlist);

	r = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER(id));

	XMMS_PLAYLIST_UNLOCK (playlist);

	if (!r)
		return NULL;

	xmms_playlist_entry_ref ((xmms_playlist_entry_t *)r->data);

	return r->data;

}

void
xmms_playlist_clear (xmms_playlist_t *playlist)
{
	GList *node;
	xmms_playlist_changed_msg_t *chmsg;

	g_return_if_fail (playlist);

	XMMS_PLAYLIST_LOCK (playlist);

	for (node = playlist->list; node; node = g_list_next (node)) {
		xmms_playlist_entry_t *entry = node->data;
		xmms_playlist_entry_unref (entry);
	}

	g_list_free (playlist->list);
	playlist->list = NULL;
	g_hash_table_destroy (playlist->id_table);
	playlist->id_table = g_hash_table_new (g_direct_hash, g_direct_equal);

	playlist->currententry = NULL;

	XMMS_PLAYLIST_UNLOCK (playlist);

	chmsg = g_new0 (xmms_playlist_changed_msg_t, 1);
	chmsg->type = XMMS_PLAYLIST_CHANGED_CLEAR;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, chmsg);


}

/** Get next entry.
 *
 *  The playlist holds a pointer to the last "taken" entry from it.
 *  xmms_playlist_get_next will return the current entry and move
 *  the pointer forward in the list.
 */

xmms_playlist_entry_t *
xmms_playlist_get_next_entry (xmms_playlist_t *playlist)
{
	xmms_playlist_entry_t *r=NULL;
	GList *n = NULL;

	g_return_val_if_fail (playlist, NULL);

	XMMS_PLAYLIST_LOCK (playlist);
	if (playlist->currententry) {
		n = g_list_next (playlist->currententry);
	} else {
		n = playlist->list;
	}

	if (n) {
		r = n->data;
		xmms_playlist_entry_ref (r);
	}

	playlist->currententry = n;

	XMMS_PLAYLIST_UNLOCK (playlist);

	
	return r;

}

xmms_playlist_entry_t *
xmms_playlist_get_prev_entry (xmms_playlist_t *playlist)
{
	xmms_playlist_entry_t *r=NULL;
	GList *n = NULL;

	g_return_val_if_fail (playlist, NULL);

	XMMS_PLAYLIST_LOCK (playlist);
	if (playlist->currententry) {
		n = g_list_previous (playlist->currententry);
	} else {
		n = playlist->list;
	}

	if (n) {
		r = n->data;
		xmms_playlist_entry_ref (r);
	}

	playlist->currententry = n;

	XMMS_PLAYLIST_UNLOCK (playlist);

	
	return r;

}

xmms_playlist_entry_t *
xmms_playlist_get_current_entry (xmms_playlist_t *playlist)
{
	xmms_playlist_entry_t *r=NULL;
	GList *n = NULL;

	g_return_val_if_fail (playlist, NULL);

	XMMS_PLAYLIST_LOCK (playlist);

	if (playlist->currententry) {
		n = playlist->currententry;
	} else {
		n = playlist->list;
	}

	if (n) {
		r = n->data;
		xmms_playlist_entry_ref (r);
	}

	playlist->currententry = n;

	XMMS_PLAYLIST_UNLOCK (playlist);

	
	return r;

}

/** Set the nextentry pointer in the playlist.
 *
 *  This will set the pointer for the next entry to be
 *  returned by xmms_playlist_get_next. This function
 *  will also wake xmms_playlist_wait
 */
  
gboolean
xmms_playlist_set_current_position (xmms_playlist_t *playlist, guint id)
{
	xmms_playlist_changed_msg_t *chmsg;
	g_return_val_if_fail (playlist, FALSE);

	XMMS_PLAYLIST_LOCK (playlist);

	playlist->currententry = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER(id));

	chmsg = g_new0 (xmms_playlist_changed_msg_t, 1);
	chmsg->type = XMMS_PLAYLIST_CHANGED_SET_POS;
	chmsg->id = id;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, chmsg);
	
	g_cond_signal (playlist->cond);

	XMMS_PLAYLIST_UNLOCK (playlist);

	return TRUE;
}


/** Ask where the nextsong pointer is.
 *
 *  @retruns a position in the playlist where we "are" now.
 */ 

gint
xmms_playlist_get_current_position (xmms_playlist_t *playlist)
{
	gint r;

	g_return_val_if_fail (playlist, 0);

	XMMS_PLAYLIST_LOCK (playlist);

	r = g_list_position (playlist->list, playlist->currententry);

	XMMS_PLAYLIST_UNLOCK (playlist);

	return r;
}

static gint
xmms_playlist_entry_compare (gconstpointer a, gconstpointer b, gpointer data)
{
	gchar *prop = data;
	const xmms_playlist_entry_t *entry1 = a;
	const xmms_playlist_entry_t *entry2 = b;
	gchar *tmpa=NULL, *tmpb=NULL;

	tmpa = xmms_playlist_entry_get_prop (entry1, prop);
	tmpb = xmms_playlist_entry_get_prop (entry2, prop);

	if (g_strcasecmp (tmpa, tmpb) == 0)
		return 0;

	return 1;
}

/** Sorts the playlist by properties.
 *
 *  This will sort the list.
 *  @param property tells xmms_playlist_sort which property it
 *  should use when sorting. 
 */

void
xmms_playlist_sort (xmms_playlist_t *playlist, gchar *property)
{
	g_return_if_fail (playlist);

	XMMS_PLAYLIST_LOCK (playlist);

	playlist->list = g_list_sort_with_data (playlist->list, xmms_playlist_entry_compare, property);

	XMMS_PLAYLIST_UNLOCK (playlist);

}

/** Lists the current playlist.
 *
 *  @returns A newly allocated GList with the current playlist.
 *  Remeber that it is only the LIST that is copied. Not the entries.
 */

GList *
xmms_playlist_list (xmms_playlist_t *playlist)
{
	GList *r=NULL, *node;
	XMMS_PLAYLIST_LOCK (playlist);

	for (node = playlist->list; node; node = g_list_next (node)) {
		r = g_list_append (r, node->data);
	}

	XMMS_PLAYLIST_UNLOCK (playlist);

	return r;
}

/** initializes a new xmms_playlist_t.
  */

xmms_playlist_t *
xmms_playlist_init ()
{
	xmms_playlist_t *ret;

	ret = g_new0 (xmms_playlist_t, 1);
	ret->cond = g_cond_new ();
	ret->mutex = g_mutex_new ();
	ret->list = NULL;
	ret->currententry = NULL;
	ret->nextid = 1;
	ret->id_table = g_hash_table_new (g_direct_hash, g_direct_equal);
	ret->is_waiting = FALSE;
	xmms_object_init (XMMS_OBJECT (ret));

	return ret;
}


/** Free the playlist and other memory in the xmms_playlist_t
 *
 *  This will free all entries in the list!
 */

void
xmms_playlist_close (xmms_playlist_t *playlist)
{
	GList *node;

	g_return_if_fail (playlist);

	g_cond_free (playlist->cond);
	g_mutex_free (playlist->mutex);

	node = playlist->list;
	
	while (node) {
		xmms_playlist_entry_t *entry = node->data;
		xmms_playlist_entry_unref (entry);
		node = g_list_next (node);
	}

	g_list_free (playlist->list);
}

/* playlist_entry_* */

xmms_playlist_entry_t *
xmms_playlist_entry_new (gchar *uri)
{
	xmms_playlist_entry_t *ret;

	ret = g_new0 (xmms_playlist_entry_t, 1);
	ret->uri = g_strdup (uri);
	ret->properties = g_hash_table_new (g_str_hash, g_str_equal);
	ret->id = 0;
	ret->ref = 1;

	return ret;
}

guint
xmms_playlist_entry_id_get (xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (entry, 0);

	return entry->id;
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
		xmms_playlist_entry_get_prop (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST),
		xmms_playlist_entry_get_prop (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM),
		xmms_playlist_entry_get_prop (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE),
		xmms_playlist_entry_get_prop (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_YEAR),
		xmms_playlist_entry_get_prop (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TRACKNR),
		xmms_playlist_entry_get_prop (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE),
		xmms_playlist_entry_get_prop (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_COMMENT),
		xmms_playlist_entry_get_prop (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION));
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
xmms_playlist_entry_get_uri (const xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (entry, NULL);

	return entry->uri;
}

gchar *
xmms_playlist_entry_get_prop (const xmms_playlist_entry_t *entry, gchar *key)
{
	g_return_val_if_fail (entry, NULL);
	g_return_val_if_fail (key, NULL);

	return g_hash_table_lookup (entry->properties, key);
}

gint
xmms_playlist_entry_get_prop_int (const xmms_playlist_entry_t *entry, gchar *key)
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

static void
xmms_playlist_entry_free (xmms_playlist_entry_t *entry)
{
	g_free (entry->uri);
	
	if (entry->mimetype)
		g_free (entry->mimetype);

	g_hash_table_foreach_remove (entry->properties, xmms_playlist_entry_foreach_free, NULL);
	g_hash_table_destroy (entry->properties);

	g_free (entry);
}

static gchar *wellknowns[] = {
	XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST,
	XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM,
	XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_YEAR,
	XMMS_PLAYLIST_ENTRY_PROPERTY_TRACKNR,
	XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_COMMENT,
	XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION,
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

void
xmms_playlist_entry_mimetype_set (xmms_playlist_entry_t *entry, const gchar *mimetype)
{

	g_return_if_fail (entry);
	g_return_if_fail (mimetype);

	entry->mimetype = g_strdup (mimetype);

}

const gchar *
xmms_playlist_entry_mimetype_get (xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (entry, NULL);

	return entry->mimetype;
}

void
xmms_playlist_entry_ref (xmms_playlist_entry_t *entry)
{
	g_return_if_fail (entry);
	entry->ref ++;
}

void
xmms_playlist_entry_unref (xmms_playlist_entry_t *entry)
{

	g_return_if_fail (entry);

	entry->ref --;

	if (entry->ref < 1) {
		/* free entry */

		XMMS_DBG("Freeing %s", entry->uri);
		xmms_playlist_entry_free (entry);
	}

}

