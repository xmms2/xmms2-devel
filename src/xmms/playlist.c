
/** @file
 *  Controls playlist
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "playlist.h"
#include "playlist_entry.h"
#include "util.h"
#include "core.h"
#include "signal_xmms.h"


/** Playlist structure */
struct xmms_playlist_St {
	xmms_object_t object;
	/** The current list first node. */
	GList *list;
	/** Next song that will be retured by xmms_playlist_get_next */
	GList *currententry;

	GHashTable *id_table;
	guint nextid;

	GMutex *mutex;
	GCond *cond;
	gboolean is_waiting;

	/** XMMS_PLAYLIST_MODE_[NONE|ALL|ONE|STOP] */
	xmms_playlist_mode_t next_mode;
};

/*
 * Public functions
 */

#define XMMS_PLAYLIST_LOCK(a) g_mutex_lock (a->mutex)
#define XMMS_PLAYLIST_UNLOCK(a) g_mutex_unlock (a->mutex)



/**
 * Sets the mode of the playlist.
 */
void
xmms_playlist_mode_set (xmms_playlist_t *playlist, xmms_playlist_mode_t mode)
{
	XMMS_PLAYLIST_LOCK (playlist);
	playlist->next_mode = mode;
	XMMS_PLAYLIST_UNLOCK (playlist);
}

/**
 * Get the current mode of the playlist.
 */
xmms_playlist_mode_t
xmms_playlist_mode_get (xmms_playlist_t *playlist)
{
	xmms_playlist_mode_t m;

	XMMS_PLAYLIST_LOCK (playlist);
	m = playlist->next_mode;
	XMMS_PLAYLIST_UNLOCK (playlist);
	
	return m;
}

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
	xmms_playlist_changed_msg_t chmsg;

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

	memset (&chmsg, 0, sizeof (xmms_playlist_changed_msg_t));
	chmsg.type = XMMS_PLAYLIST_CHANGED_SHUFFLE;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, &chmsg);

	g_cond_signal (playlist->cond);

	XMMS_PLAYLIST_UNLOCK (playlist);
}

/** removes entry from playlist */

gboolean 
xmms_playlist_id_remove (xmms_playlist_t *playlist, guint id)
{
	GList *node;
	xmms_playlist_changed_msg_t chmsg;

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

	memset (&chmsg, 0, sizeof (xmms_playlist_changed_msg_t));
	chmsg.type = XMMS_PLAYLIST_CHANGED_REMOVE;
	chmsg.id = id;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, &chmsg);

	g_cond_signal (playlist->cond);
	
	XMMS_PLAYLIST_UNLOCK (playlist);

	return TRUE;
}

/** move entry in playlist */

gboolean
xmms_playlist_id_move (xmms_playlist_t *playlist, guint id, gint steps)
{
	xmms_playlist_entry_t *entry;
	xmms_playlist_changed_msg_t chmsg;
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

	memset (&chmsg, 0, sizeof (xmms_playlist_changed_msg_t));
	chmsg.type = XMMS_PLAYLIST_CHANGED_MOVE;
	chmsg.id = id;
	chmsg.arg = GINT_TO_POINTER (steps);
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, &chmsg);

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
	xmms_playlist_changed_msg_t chmsg;

	g_return_val_if_fail (file, FALSE);

	new = g_list_alloc ();
	new->data = (gpointer) file;

	XMMS_PLAYLIST_LOCK (playlist);

	xmms_playlist_entry_id_set (file, playlist->nextid++);
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
	
	g_hash_table_insert (playlist->id_table, 
			     GUINT_TO_POINTER (xmms_playlist_entry_id_get (file)), 
			     new);

	memset (&chmsg, 0, sizeof (xmms_playlist_changed_msg_t));
	chmsg.type = XMMS_PLAYLIST_CHANGED_ADD;
	chmsg.id = xmms_playlist_entry_id_get (file);
	chmsg.arg = GINT_TO_POINTER (options);
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, &chmsg);

	g_cond_signal (playlist->cond);

	XMMS_PLAYLIST_UNLOCK (playlist);

	xmms_core_mediainfo_add_entry (xmms_playlist_entry_id_get (file));

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
	xmms_playlist_changed_msg_t chmsg;

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

	memset (&chmsg, 0, sizeof (xmms_playlist_changed_msg_t));
	chmsg.type = XMMS_PLAYLIST_CHANGED_CLEAR;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, &chmsg);

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
		if (playlist->next_mode == XMMS_PLAYLIST_MODE_REPEAT_ONE) {
			n = playlist->currententry;
		} else {
			n = g_list_next (playlist->currententry);
		}
	} else {
		n = playlist->list;
	}

	if (n) {
		r = n->data;
		xmms_playlist_entry_ref (r);
	}

	playlist->currententry = n;

	XMMS_PLAYLIST_UNLOCK (playlist);
	
	if (playlist->next_mode == XMMS_PLAYLIST_MODE_STOP)
		return NULL;

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
	xmms_playlist_changed_msg_t chmsg;
	g_return_val_if_fail (playlist, FALSE);

	XMMS_PLAYLIST_LOCK (playlist);

	playlist->currententry = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER(id));

	memset (&chmsg, 0, sizeof (xmms_playlist_changed_msg_t));
	chmsg.type = XMMS_PLAYLIST_CHANGED_SET_POS;
	chmsg.id = id;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, &chmsg);

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

	tmpa = xmms_playlist_entry_property_get (entry1, prop);
	tmpb = xmms_playlist_entry_property_get (entry2, prop);

	XMMS_DBG ("CMP %s with %s", tmpa, tmpb);

	return g_strcasecmp (tmpa, tmpb);
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
	xmms_playlist_changed_msg_t chmsg;

	g_return_if_fail (playlist);
	XMMS_DBG ("Sorting on %s", property);

	XMMS_PLAYLIST_LOCK (playlist);

	playlist->list = g_list_sort_with_data (playlist->list, xmms_playlist_entry_compare, property);

	XMMS_PLAYLIST_UNLOCK (playlist);

	memset (&chmsg, 0, sizeof (xmms_playlist_changed_msg_t));
	chmsg.type = XMMS_PLAYLIST_CHANGED_SORT;
	xmms_object_emit (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, &chmsg);

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

