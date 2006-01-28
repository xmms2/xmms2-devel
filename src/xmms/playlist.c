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


/** @file
 *  Controls playlist
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>

#include "xmmspriv/xmms_playlist.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_config.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmms/xmms_log.h"
/*
#include "xmms/plsplugins.h"
#include "xmms/util.h"
#include "xmms/signal_xmms.h"
#include "xmms/ipc.h"
#include "xmms/mediainfo.h"
#include "xmms/magic.h"
*/
static void xmms_playlist_destroy (xmms_object_t *object);
static void xmms_playlist_shuffle (xmms_playlist_t *playlist, xmms_error_t *err);
static void xmms_playlist_clear (xmms_playlist_t *playlist, xmms_error_t *err);
static void xmms_playlist_sort (xmms_playlist_t *playlist, gchar *property, xmms_error_t *err);
static void xmms_playlist_destroy (xmms_object_t *object);
gboolean xmms_playlist_remove (xmms_playlist_t *playlist, guint pos, xmms_error_t *err);
static gboolean xmms_playlist_move (xmms_playlist_t *playlist, guint pos, gint newpos, xmms_error_t *err);
static guint xmms_playlist_set_current_position_rel (xmms_playlist_t *playlist, gint32 pos, xmms_error_t *error);

static gboolean xmms_playlist_inserturl (xmms_playlist_t *playlist, guint32 pos, gchar *url, xmms_error_t *error);
static gboolean xmms_playlist_insert (xmms_playlist_t *playlist, guint32 pos, xmms_medialib_entry_t file, xmms_error_t *error);

XMMS_CMD_DEFINE (insert, xmms_playlist_inserturl, xmms_playlist_t *, NONE, UINT32, STRING);
XMMS_CMD_DEFINE (insertid, xmms_playlist_insert, xmms_playlist_t *, NONE, UINT32, UINT32);
XMMS_CMD_DEFINE (shuffle, xmms_playlist_shuffle, xmms_playlist_t *, NONE, NONE, NONE);
XMMS_CMD_DEFINE (remove, xmms_playlist_remove, xmms_playlist_t *, NONE, UINT32, NONE);
XMMS_CMD_DEFINE (move, xmms_playlist_move, xmms_playlist_t *, NONE, UINT32, INT32);
XMMS_CMD_DEFINE (add, xmms_playlist_addurl, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (addid, xmms_playlist_add, xmms_playlist_t *, NONE, UINT32, NONE);
XMMS_CMD_DEFINE (clear, xmms_playlist_clear, xmms_playlist_t *, NONE, NONE, NONE);
XMMS_CMD_DEFINE (sort, xmms_playlist_sort, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (list, xmms_playlist_list, xmms_playlist_t *, LIST, NONE, NONE);
XMMS_CMD_DEFINE (current_pos, xmms_playlist_current_pos, xmms_playlist_t *, UINT32, NONE, NONE);
XMMS_CMD_DEFINE (set_pos, xmms_playlist_set_current_position, xmms_playlist_t *, UINT32, UINT32, NONE);
XMMS_CMD_DEFINE (set_pos_rel, xmms_playlist_set_current_position_rel, xmms_playlist_t *, UINT32, INT32, NONE);

static GHashTable *
xmms_playlist_changed_msg_new (xmms_playlist_changed_actions_t type, guint32 id)
{
	GHashTable *dict;
	xmms_object_cmd_value_t *val;
	dict = g_hash_table_new_full (g_str_hash, 
				      g_str_equal, 
				      NULL,
				      xmms_object_cmd_value_free);
	val = xmms_object_cmd_value_int_new (type);
	g_hash_table_insert (dict, "type", val);
	if (id) {
		val = xmms_object_cmd_value_uint_new (id);
		g_hash_table_insert (dict, "id", val);
	}
	return dict;
}

static void
xmms_playlist_changed_msg_send (xmms_playlist_t *playlist, GHashTable *dict)
{
	g_return_if_fail (playlist);
	g_return_if_fail (dict);

	xmms_object_emit_f (XMMS_OBJECT (playlist),
			    XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
			    XMMS_OBJECT_CMD_ARG_DICT,
			    dict);

	g_hash_table_destroy (dict);
}

#define XMMS_PLAYLIST_CHANGED_MSG(type, id) xmms_playlist_changed_msg_send (playlist, xmms_playlist_changed_msg_new (type, id))


/** @defgroup Playlist Playlist
  * @ingroup XMMSServer
  * @brief This is the playlist control.
  *
  * A playlist is a central thing in the XMMS server, it
  * tells us what to do after we played the following entry
  * @{
  */

/** Playlist structure */
struct xmms_playlist_St {
	xmms_object_t object;

	/* the list is an array */
	GArray *list;

	gint32 currentpos;

	gboolean repeat_one;
	gboolean repeat_all;

	GMutex *mutex;

	xmms_mediainfo_reader_t *mediainfordr;

};


static void
on_playlist_r_all_changed (xmms_object_t *object, gconstpointer data,
			   gpointer udata)
{
	xmms_playlist_t *playlist = udata;

	g_mutex_lock (playlist->mutex);
	if (data) 
		playlist->repeat_all = atoi ((gchar *)data);
	g_mutex_unlock (playlist->mutex);
}

static void
on_playlist_r_one_changed (xmms_object_t *object, gconstpointer data,
			   gpointer udata)
{
	xmms_playlist_t *playlist = udata;

	g_mutex_lock (playlist->mutex);
	if (data)
		playlist->repeat_one = atoi ((gchar *)data);
	g_mutex_unlock (playlist->mutex);
}

/**
 * Initializes a new xmms_playlist_t.
 */
xmms_playlist_t *
xmms_playlist_init (void)
{
	xmms_playlist_t *ret;
	xmms_config_property_t *val, *load_autosaved;

	ret = xmms_object_new (xmms_playlist_t, xmms_playlist_destroy);
	ret->mutex = g_mutex_new ();
	ret->list = g_array_new (FALSE, FALSE, sizeof (guint32));
	ret->currentpos = -1; /* we start with an invalid entry */

	xmms_ipc_object_register (XMMS_IPC_OBJECT_PLAYLIST, XMMS_OBJECT (ret));

	xmms_ipc_broadcast_register (XMMS_OBJECT (ret),
	                             XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
	xmms_ipc_broadcast_register (XMMS_OBJECT (ret),
	                             XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);

	val = xmms_config_property_register ("playlist.repeat_one", "0",
	                                     on_playlist_r_one_changed, ret);
	ret->repeat_one = xmms_config_property_get_int (val);
	
	val = xmms_config_property_register ("playlist.repeat_all", "0",
	                                  on_playlist_r_all_changed, ret);
	ret->repeat_all = xmms_config_property_get_int (val);




	load_autosaved =
		xmms_config_property_register ("playlist.load_autosaved", "1",
		                            NULL, NULL);

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_CURRENT_POS, 
			     XMMS_CMD_FUNC (current_pos));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_SHUFFLE, 
			     XMMS_CMD_FUNC (shuffle));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_SET_POS, 
			     XMMS_CMD_FUNC (set_pos));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_SET_POS_REL,
			     XMMS_CMD_FUNC (set_pos_rel));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_ADD, 
			     XMMS_CMD_FUNC (add));
	
	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_ADD_ID, 
			     XMMS_CMD_FUNC (addid));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_REMOVE, 
			     XMMS_CMD_FUNC (remove));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_MOVE, 
			     XMMS_CMD_FUNC (move));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_LIST, 
			     XMMS_CMD_FUNC (list));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_CLEAR, 
			     XMMS_CMD_FUNC (clear));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_SORT, 
			     XMMS_CMD_FUNC (sort));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_INSERT, 
			     XMMS_CMD_FUNC (insert));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_INSERT_ID, 
			     XMMS_CMD_FUNC (insertid));

	xmms_medialib_init (ret);

	ret->mediainfordr = xmms_mediainfo_reader_start (ret);

	if (xmms_config_property_get_int (load_autosaved)) {
		xmms_medialib_playlist_load_autosaved ();
	}

	return ret;
}

/**
 * Go to next song in playlist according to current playlist mode.
 * xmms_playlist_current_entry is to be used to retrieve the entry.
 *
 * @sa xmms_playlist_current_entry
 *
 * @returns FALSE if end of playlist is reached, TRUE otherwise.
 */
gboolean
xmms_playlist_advance (xmms_playlist_t *playlist)
{
	gboolean ret = TRUE;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	if (playlist->list->len == 0) {
		ret = FALSE;
	} else if (!playlist->repeat_one) {
		playlist->currentpos++;
		playlist->currentpos %= playlist->list->len;
		ret = (playlist->currentpos != 0) || playlist->repeat_all;
		xmms_object_emit_f (XMMS_OBJECT (playlist), XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS, XMMS_OBJECT_CMD_ARG_UINT32, playlist->currentpos);
	}
	g_mutex_unlock (playlist->mutex);

	return ret;
}

/**
 * Retrive the currently active xmms_medialib_entry_t.
 *
 */
xmms_medialib_entry_t
xmms_playlist_current_entry (xmms_playlist_t *playlist)
{
	xmms_medialib_entry_t ent = 0;

	g_return_val_if_fail (playlist, 0);
	
	g_mutex_lock (playlist->mutex);

	if (playlist->currentpos == -1 && (playlist->list->len > 0)) {
		playlist->currentpos = 0;
		xmms_object_emit_f (XMMS_OBJECT (playlist),
				    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
				    XMMS_OBJECT_CMD_ARG_UINT32, 0);
	}

	if (playlist->currentpos < playlist->list->len) {
		ent = g_array_index (playlist->list, guint32, playlist->currentpos);
	} else {
		ent = 0;
	}

	g_mutex_unlock (playlist->mutex);

	return ent;
}


/**
 * Retrieve the position of the currently active xmms_medialib_entry_t
 *
 */
guint32
xmms_playlist_current_pos (xmms_playlist_t *playlist, xmms_error_t *error)
{
	guint32 pos;
	g_return_val_if_fail (playlist, 0);
	
	g_mutex_lock (playlist->mutex);

	pos = playlist->currentpos;
	if (pos == -1) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "no current entry");
	}

	g_mutex_unlock (playlist->mutex);

	return pos;
}

static inline void
swap_entries(GArray *l, gint i, gint j)
{
	guint32 tmp;
	tmp = g_array_index (l, guint32, i);
	g_array_index (l, guint32, i) = g_array_index (l, guint32, j);
	g_array_index (l, guint32, j) = tmp;
}


/**
 * Shuffle the playlist.
 *
 */
static void
xmms_playlist_shuffle (xmms_playlist_t *playlist, xmms_error_t *err)
{
	guint j,i;
	gint len;

	g_return_if_fail (playlist);

	g_mutex_lock (playlist->mutex);

	len = playlist->list->len;
	if (len > 1) {

		/* put current at top and exclude from shuffling */
		if (playlist->currentpos != -1) {
			swap_entries (playlist->list, 0, playlist->currentpos);
			playlist->currentpos = 0;
		}

		/* knuth <3 */
		for (i = 1; i < len; i++) {
			j = g_random_int_range (i, len);
			
			swap_entries (playlist->list, i, j);
		}

	}

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SHUFFLE, 0);
	xmms_object_emit_f (XMMS_OBJECT (playlist), XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS, XMMS_OBJECT_CMD_ARG_UINT32, playlist->currentpos);

	g_mutex_unlock (playlist->mutex);
}

static gboolean
xmms_playlist_remove_unlocked (xmms_playlist_t *playlist, guint pos, xmms_error_t *err)
{
	GHashTable *dict;

	g_return_val_if_fail (playlist, FALSE);

	if (pos >= playlist->list->len) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	g_array_remove_index (playlist->list, pos);

	/* decrease currentpos if removed entry was before or if it's
	 * the current entry, but only if currentpos is a valid entry.
	 */
	if (playlist->currentpos != -1 && pos <= playlist->currentpos) {
		playlist->currentpos--;
	}

	dict = xmms_playlist_changed_msg_new (XMMS_PLAYLIST_CHANGED_REMOVE, 0);
	g_hash_table_insert (dict, "position", xmms_object_cmd_value_int_new (pos));
	xmms_playlist_changed_msg_send (playlist, dict);
	
	xmms_object_emit_f (XMMS_OBJECT (playlist), XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS, XMMS_OBJECT_CMD_ARG_INT32, playlist->currentpos);

	return TRUE;
}

/**
 * Remove all additions of #entry in the playlist
 * @sa xmms_playlist_remove
 */
gboolean
xmms_playlist_remove_by_entry (xmms_playlist_t *playlist, xmms_medialib_entry_t entry)
{
	guint32 i;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	for (i = 0; i < playlist->list->len; i++) {
		if (g_array_index (playlist->list, guint32, i) == entry) {
			XMMS_DBG ("removing entry on pos %d", i);
			xmms_playlist_remove_unlocked (playlist, i, NULL);
			i--; /* reset it */
		}
	}

	g_mutex_unlock (playlist->mutex);

	return TRUE;
}

/**
 * Remove an entry from playlist.
 *
 */
gboolean 
xmms_playlist_remove (xmms_playlist_t *playlist, guint pos, xmms_error_t *err)
{
	gboolean ret;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);
	ret = xmms_playlist_remove_unlocked (playlist, pos, err);
	g_mutex_unlock (playlist->mutex);
	return ret;
}


/**
 * Move an entry in playlist
 *
 */
static gboolean
xmms_playlist_move (xmms_playlist_t *playlist, guint pos, gint newpos, xmms_error_t *err)
{
	GHashTable *dict;
	guint32 id;

	g_return_val_if_fail (playlist, FALSE);

	XMMS_DBG ("Moving %d, to %d", pos, newpos);

	g_mutex_lock (playlist->mutex);
	
	if (playlist->list->len == 0 || newpos > (playlist->list->len - 1)) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Cannot move entry outside playlist");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}
	
	id = g_array_index (playlist->list, guint32, pos);
	if (!id) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	g_array_remove_index (playlist->list, pos);
	g_array_insert_val (playlist->list, newpos, id);

	if (newpos <= playlist->currentpos && pos > playlist->currentpos)
		playlist->currentpos++;
	else if (newpos >= playlist->currentpos && pos < playlist->currentpos)
		playlist->currentpos--;
	else if (pos == playlist->currentpos)
		playlist->currentpos = newpos;

	dict = xmms_playlist_changed_msg_new (XMMS_PLAYLIST_CHANGED_MOVE, id);
	g_hash_table_insert (dict, "position", xmms_object_cmd_value_int_new (pos));
	g_hash_table_insert (dict, "newposition", xmms_object_cmd_value_int_new (newpos));
	xmms_playlist_changed_msg_send (playlist, dict);

	xmms_object_emit_f (XMMS_OBJECT (playlist), XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS, XMMS_OBJECT_CMD_ARG_UINT32, playlist->currentpos);

	g_mutex_unlock (playlist->mutex);

	return TRUE;

}

/**
 * Insert an entry into the playlist at given position.
 * Creates a #xmms_medialib_entry for you and insert it
 * in the list.
 *
 * @param playlist the playlist to add it URL to.
 * @param pos the position where the entry is inserted.
 * @param url the URL to add.
 * @param err an #xmms_error_t that should be defined upon error.
 * @return TRUE on success and FALSE otherwise.
 *
 */
static gboolean
xmms_playlist_inserturl (xmms_playlist_t *playlist, guint32 pos, gchar *url, xmms_error_t *err)
{
	xmms_medialib_entry_t entry = 0;
	xmms_medialib_session_t *session = xmms_medialib_begin();

	entry = xmms_medialib_entry_new_encoded (session, url, err);
	xmms_medialib_end (session);

	if (!entry) {
		return FALSE;
	}

	return xmms_playlist_insert (playlist, pos, entry, err);
}

/**
 * Insert an xmms_medialib_entry to the playlist at given position.
 *
 * @param playlist the playlist to add the entry to.
 * @param pos the position where the entry is inserted.
 * @param file the #xmms_medialib_entry to add.
 * @param error Upon error this will be set.
 * @returns TRUE on success and FALSE otherwise.
 */
static gboolean
xmms_playlist_insert (xmms_playlist_t *playlist, guint32 pos, xmms_medialib_entry_t file, xmms_error_t *err)
{
	GHashTable *dict;
	gint len;
	g_return_val_if_fail (file, FALSE);

	g_mutex_lock (playlist->mutex);
	len = playlist->list->len;
	if (len != 0)
		len --;

	if (pos > len || pos < 0) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Could not insert entry outside of playlist!");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}
	g_array_insert_val (playlist->list, pos, file);

	/** propagate the MID ! */
	dict = xmms_playlist_changed_msg_new (XMMS_PLAYLIST_CHANGED_INSERT, file);
	g_hash_table_insert (dict, "position", xmms_object_cmd_value_int_new (pos));
	xmms_playlist_changed_msg_send (playlist, dict);
	
	g_mutex_unlock (playlist->mutex);
	return TRUE;
}

/**
  * Convenient function for adding a URL to the playlist,
  * Creates a #xmms_medialib_entry for you and adds it
  * to the list.
  *
  * @param playlist the playlist to add it URL to.
  * @param nurl the URL to add
  * @param err an #xmms_error_t that should be defined upon error.
  * @return TRUE on success and FALSE otherwise.
  */
gboolean
xmms_playlist_addurl (xmms_playlist_t *playlist, gchar *nurl, xmms_error_t *err)
{
	gboolean res;
	xmms_medialib_entry_t entry = 0;
	xmms_medialib_session_t *session = xmms_medialib_begin();
	
	entry = xmms_medialib_entry_new_encoded (session, nurl, err);
	xmms_medialib_end (session);

	if (!entry) {
		return FALSE;
	}

	res = xmms_playlist_add (playlist, entry, err);

	return res;
}

/** Adds a xmms_medialib_entry to the playlist.
 *
 *  This will append or prepend the entry according to
 *  the option.
 *  This function will wake xmms_playlist_wait.
 *  @param playlist the playlist to add the entry to.
 *  @param file the #xmms_medialib_entry to add
 *  @param error Upon error this will be set. 
 *  @returns TRUE on success
 */

gboolean
xmms_playlist_add (xmms_playlist_t *playlist, xmms_medialib_entry_t file, xmms_error_t *error)
{
	GHashTable *dict;
	g_return_val_if_fail (file, FALSE);

	if (!xmms_medialib_check_id (file)) {
		if (error) {
			/* we can be called internaly also! */
			xmms_error_set (error, XMMS_ERROR_NOENT, 
							"That is not a valid medialib id!");
		}
		return FALSE;
	}

	g_mutex_lock (playlist->mutex);
	g_array_append_val (playlist->list, file);

	/** propagate the MID ! */
	dict = xmms_playlist_changed_msg_new (XMMS_PLAYLIST_CHANGED_ADD, file);
	g_hash_table_insert (dict, "position", xmms_object_cmd_value_int_new (playlist->list->len-1));
	xmms_playlist_changed_msg_send (playlist, dict);

	g_mutex_unlock (playlist->mutex);

	return TRUE;

}

/** Clear the playlist */
static void
xmms_playlist_clear (xmms_playlist_t *playlist, xmms_error_t *err)
{
	g_return_if_fail (playlist);

	g_mutex_lock (playlist->mutex);

	g_array_free (playlist->list, FALSE);
	playlist->list = g_array_new (FALSE, FALSE, sizeof (guint32));
	playlist->currentpos = -1;

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_CLEAR, 0);
	g_mutex_unlock (playlist->mutex);

}


/** Set the nextentry pointer in the playlist.
 *
 *  This will set the pointer for the next entry to be
 *  returned by xmms_playlist_advance. This function
 *  will also wake xmms_playlist_wait
 */

static guint
xmms_playlist_set_current_position_do (xmms_playlist_t *playlist, guint32 pos, xmms_error_t *error)
{
	guint mid;
	g_return_val_if_fail (playlist, FALSE);

	if (pos >= playlist->list->len) {
		xmms_error_set (error, XMMS_ERROR_INVAL, "Can't set pos outside the current playlist!");
		return 0;
	}

	XMMS_DBG ("newpos! %d", pos);
	playlist->currentpos = pos;

	xmms_object_emit_f (XMMS_OBJECT (playlist), XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS, XMMS_OBJECT_CMD_ARG_UINT32, playlist->currentpos);

	mid = g_array_index (playlist->list, guint32, playlist->currentpos);

	return mid;
}

guint
xmms_playlist_set_current_position (xmms_playlist_t *playlist, guint32 pos, xmms_error_t *error)
{
	guint mid;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);
	mid = xmms_playlist_set_current_position_do (playlist, pos, error);
	g_mutex_unlock (playlist->mutex);

	return mid;
}

static guint
xmms_playlist_set_current_position_rel (xmms_playlist_t *playlist, gint32 pos, xmms_error_t *error)
{
	guint mid = 0;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	if (playlist->currentpos + pos >= 0)
		mid = xmms_playlist_set_current_position_do (playlist, playlist->currentpos + pos, error);

	g_mutex_unlock (playlist->mutex);

	return mid;
}

typedef struct {
	guint id;
	xmms_object_cmd_value_t *val;
	gboolean current;
} sortdata_t;


/**
 * Sort helper function.
 * Performs a case insesitive comparation between two entries.
 */
static gint
xmms_playlist_entry_compare (gconstpointer a, gconstpointer b)
{
	sortdata_t *data1 = (sortdata_t *) a;
	sortdata_t *data2 = (sortdata_t *) b;
	int s1, s2;

	if (!data1->val) {
		return -(data2->val != NULL);
	}

	if (!data2->val) {
		return 1;
	}

	if (data1->val->type == XMMS_OBJECT_CMD_ARG_STRING &&
	    data2->val->type == XMMS_OBJECT_CMD_ARG_STRING) {
		return g_utf8_collate (data1->val->value.string,
		                       data2->val->value.string);
	}

	if ((data1->val->type == XMMS_OBJECT_CMD_ARG_INT32 ||
	     data1->val->type == XMMS_OBJECT_CMD_ARG_UINT32) &&
	    (data2->val->type == XMMS_OBJECT_CMD_ARG_INT32 ||
	     data2->val->type == XMMS_OBJECT_CMD_ARG_UINT32))
	{
		s1 = (data1->val->type == XMMS_OBJECT_CMD_ARG_INT32) ?
		      data1->val->value.int32 : data1->val->value.uint32;
		s2 = (data2->val->type == XMMS_OBJECT_CMD_ARG_INT32) ?
		      data2->val->value.int32 : data2->val->value.uint32;

		if (s1 < s2)
			return -1;
		else if (s1 > s2)
			return 1;
		else
			return 0;
	}

	XMMS_DBG("Types in compare function differ to much");

	return 0;
}

/**
 * Unwind helper function.
 * Fills the playlist with the new sorted data.
 */
static void
xmms_playlist_sorted_unwind (gpointer data, gpointer userdata)
{
	sortdata_t *sorted = (sortdata_t *) data;
	xmms_playlist_t *playlist = (xmms_playlist_t *)userdata;

	g_array_append_val (playlist->list, sorted->id);

	if (sorted->val) {
		xmms_object_cmd_value_free (sorted->val);
	}

	if (sorted->current) {
		playlist->currentpos = playlist->list->len - 1;
	}

	g_free (sorted);
}

/** Sorts the playlist by properties.
 *
 *  This will sort the list.
 *  @param playlist The playlist to sort.
 *  @param property Tells xmms_playlist_sort which property it
 *  should use when sorting.
 *  @param err An #xmms_error_t - needed since xmms_playlist_sort is an ipc
 *  method handler.
 */

static void
xmms_playlist_sort (xmms_playlist_t *playlist, gchar *property, xmms_error_t *err)
{
	guint32 i;
	GList *tmp = NULL, *sorted, *l, *l2;
	sortdata_t *data;
	gchar *str;
	xmms_medialib_session_t *session;
	gboolean list_changed = FALSE;

	g_return_if_fail (playlist);
	g_return_if_fail (property);
	XMMS_DBG ("Sorting on %s", property);

	g_mutex_lock (playlist->mutex);

	/* check whether we need to do any sorting at all */
	if (playlist->list->len < 2) {
		g_mutex_unlock (playlist->mutex);
		return;
	}

	session = xmms_medialib_begin ();

	for (i = 0; i < playlist->list->len; i++) {
		data = g_new (sortdata_t, 1);

		data->id = g_array_index (playlist->list, xmms_medialib_entry_t, i);
		data->val =
			xmms_medialib_entry_property_get_cmd_value (session,
			                                            data->id,
			                                            property);

		if (data->val && data->val->type == XMMS_OBJECT_CMD_ARG_STRING) {
			str = data->val->value.string;
			data->val->value.string = g_utf8_casefold (str, strlen (str));
			g_free (str);
		}

		data->current = (playlist->currentpos == i);

		tmp = g_list_prepend (tmp, data);
	}

	xmms_medialib_end (session);

	tmp = g_list_reverse (tmp);
	sorted = g_list_sort (tmp, xmms_playlist_entry_compare);

	/* check whether there was any change */
	for (l = sorted, l2 = tmp; l;
	     l = g_list_next (l), l2 = g_list_next (l2)) {
		if (!l2 || l->data != l2->data) {
			list_changed = TRUE;
			break;
		}
	}

	if (!list_changed) {
		g_list_free (tmp);
		g_mutex_unlock (playlist->mutex);
		return;
	}

	g_array_set_size (playlist->list, 0);
	g_list_foreach (sorted, xmms_playlist_sorted_unwind, playlist);

	g_list_free (sorted);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SORT, 0);

	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                    XMMS_OBJECT_CMD_ARG_UINT32,
	                    playlist->currentpos);

	g_mutex_unlock (playlist->mutex);
}

/** Lists the current playlist.
 *
 * @returns A newly allocated GList with the current playlist.
 * Remeber that it is only the LIST that is copied. Not the entries.
 * The entries are however referenced, and must be unreffed!
 */
GList *
xmms_playlist_list (xmms_playlist_t *playlist, xmms_error_t *err)
{
	GList *r = NULL;
	guint32 i;

	g_mutex_lock (playlist->mutex);

	for (i = 0; i < playlist->list->len; i++) {
		xmms_object_cmd_value_t *val;

		val = xmms_object_cmd_value_uint_new (g_array_index (playlist->list, 
								     guint32, i));
		r = g_list_prepend (r, val);
	}

	r = g_list_reverse (r);
	
	g_mutex_unlock (playlist->mutex);

	return r;
}

/** returns pointer to mediainfo reader. */
xmms_mediainfo_reader_t *
xmms_playlist_mediainfo_reader_get (xmms_playlist_t *playlist)
{
	g_return_val_if_fail (playlist, NULL);

	return playlist->mediainfordr;
}

/** @} */

/** Free the playlist and other memory in the xmms_playlist_t
 *
 *  This will free all entries in the list!
 */

static void
xmms_playlist_destroy (xmms_object_t *object)
{
	xmms_config_property_t *val;
	xmms_playlist_t *playlist = (xmms_playlist_t *)object;

	g_return_if_fail (playlist);

	/* we need to save the playlist before we free the playlist
	 * mutex, since the following call will eventually lead to a
	 * call to the playlist object again, which will of course try
	 * to lock the mutex.
	 *
	 * it's safe to do it like this, since there's only one thread left
	 * anyway when we destroy the playlist.
	 */
	xmms_medialib_playlist_save_autosaved ();

	g_mutex_free (playlist->mutex);

	val = xmms_config_lookup ("playlist.repeat_one");
	xmms_config_property_callback_remove (val, on_playlist_r_one_changed);
	val = xmms_config_lookup ("playlist.repeat_all");
	xmms_config_property_callback_remove (val, on_playlist_r_all_changed);

	xmms_object_unref (playlist->mediainfordr);

	g_array_free (playlist->list, TRUE);

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_PLAYLIST);
}
