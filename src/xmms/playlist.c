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

#include "xmms/playlist.h"
#include "xmms/medialib.h"
#include "xmms/plsplugins.h"
#include "xmms/util.h"
#include "xmms/signal_xmms.h"
#include "xmms/ipc.h"
#include "xmms/mediainfo.h"
#include "xmms/magic.h"


/** @defgroup PlaylistClientMethods PlaylistClientMethods
  * @ingroup Playlist
  * @brief the playlist methods that could be used by the client
  */

/** @defgroup Playlist Playlist
  * @ingroup XMMSServer
  * @brief This is the playlist control.
  *
  * A playlist is a central thing in the XMMS server, it
  * tells us what to do after we played the following entry
  * @{
  */

/* Internal macro to emit XMMS_SIGNAL_PLAYLIST_CHANGED */
#define XMMS_PLAYLIST_CHANGED_MSG(ttype,iid,argument) do { \
	xmms_playlist_changed_msg_t *chmsg; \
	chmsg = g_new0 (xmms_playlist_changed_msg_t, 1);\
	chmsg->type = ttype; chmsg->id=iid; chmsg->arg=argument; \
	xmms_object_emit_f (XMMS_OBJECT (playlist), XMMS_IPC_SIGNAL_PLAYLIST_CHANGED, XMMS_OBJECT_CMD_ARG_PLCH, chmsg);\
	g_free (chmsg); \
} while (0)

/** Playlist structure */
struct xmms_playlist_St {
	xmms_object_t object;

	/* the list is an array */
	GArray *list;
	/* this holds the current lenght of list */
	guint32 list_len;

	guint32 currentpos;

	gboolean repeat_one;
	gboolean repeat_all;

	GMutex *mutex;

	xmms_mediainfo_thread_t *mediainfothr;

};


static void xmms_playlist_destroy (xmms_object_t *object);
static void xmms_playlist_shuffle (xmms_playlist_t *playlist, xmms_error_t *err);
static void xmms_playlist_clear (xmms_playlist_t *playlist, xmms_error_t *err);
static void xmms_playlist_sort (xmms_playlist_t *playlist, gchar *property, xmms_error_t *err);
static void xmms_playlist_destroy (xmms_object_t *object);
gboolean xmms_playlist_remove (xmms_playlist_t *playlist, guint pos, xmms_error_t *err);
static gboolean xmms_playlist_move (xmms_playlist_t *playlist, guint pos, gint newpos, xmms_error_t *err);
gboolean xmms_playlist_medialibadd (xmms_playlist_t *playlist, gchar *query, xmms_error_t *err);

XMMS_CMD_DEFINE (shuffle, xmms_playlist_shuffle, xmms_playlist_t *, NONE, NONE, NONE);
XMMS_CMD_DEFINE (remove, xmms_playlist_remove, xmms_playlist_t *, NONE, UINT32, NONE);
XMMS_CMD_DEFINE (move, xmms_playlist_move, xmms_playlist_t *, NONE, UINT32, INT32);
XMMS_CMD_DEFINE (add, xmms_playlist_addurl, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (clear, xmms_playlist_clear, xmms_playlist_t *, NONE, NONE, NONE);
XMMS_CMD_DEFINE (sort, xmms_playlist_sort, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (list, xmms_playlist_list, xmms_playlist_t *, UINTLIST, NONE, NONE);
XMMS_CMD_DEFINE (save, xmms_playlist_save, xmms_playlist_t *, NONE, STRING, NONE);


/** initializes a new xmms_playlist_t.
  */

xmms_playlist_t *
xmms_playlist_init (void)
{
	xmms_playlist_t *ret;

	ret = xmms_object_new (xmms_playlist_t, xmms_playlist_destroy);
	ret->mutex = g_mutex_new ();
	ret->list = g_array_new (TRUE, TRUE, sizeof (guint32));
	ret->currentpos = 0;

	xmms_ipc_object_register (XMMS_IPC_OBJECT_PLAYLIST, XMMS_OBJECT (ret));

	xmms_ipc_broadcast_register (XMMS_OBJECT (ret), XMMS_IPC_SIGNAL_PLAYLIST_MEDIAINFO_ID);
	xmms_ipc_broadcast_register (XMMS_OBJECT (ret), XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);

/*	val = xmms_config_value_register ("playlist.repeat", "none",
	                                  on_playlist_mode_changed, ret);
	tmp = xmms_config_value_string_get (val);
	ret->mode = playlist_mode_from_str (tmp);*/

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_SHUFFLE, 
			     XMMS_CMD_FUNC (shuffle));

/*	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_JUMP, 
			     XMMS_CMD_FUNC (set_next));*/

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_ADD, 
			     XMMS_CMD_FUNC (add));

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
			     XMMS_IPC_CMD_SAVE,
			     XMMS_CMD_FUNC (save));

	ret->mediainfothr = xmms_mediainfo_thread_start (ret);
	xmms_medialib_init (ret);

	return ret;
}

/** advance and return the mid for this playlist. */

xmms_medialib_entry_t
xmms_playlist_advance (xmms_playlist_t *playlist)
{
	xmms_medialib_entry_t ent;

	g_return_val_if_fail (playlist, 0);

	/** @todo maybe not fulhack the threadsaftey */
	g_mutex_lock (playlist->mutex);
	if (playlist->list->len == 0) {
		g_mutex_unlock (playlist->mutex);
		return 0;
	}

	if (playlist->repeat_one) {
		ent = g_array_index (playlist->list, guint32, playlist->currentpos);
	} else if (playlist->currentpos + 1 > playlist->list->len) {
		if (playlist->repeat_all) {
			playlist->currentpos = 1;
			ent = g_array_index (playlist->list, 
					     guint32, 
					     playlist->currentpos);
		} else {
			ent = 0;
			playlist->currentpos = 0;
		}
	} else {
		playlist->currentpos++;
		ent = g_array_index (playlist->list, 
				     guint32, 
				     playlist->currentpos);
	}
	g_mutex_unlock (playlist->mutex);

	return ent;
}

/** return the current mid */
xmms_medialib_entry_t
xmms_playlist_current_entry (xmms_playlist_t *playlist)
{
	xmms_medialib_entry_t ent;

	g_return_val_if_fail (playlist, 0);
	
	g_mutex_lock (playlist->mutex);
	ent = g_array_index (playlist->list, guint32, playlist->currentpos);
	g_mutex_unlock (playlist->mutex);

	return ent;
}

/** Total number of entries in the playlist. */
guint
xmms_playlist_entries_total (xmms_playlist_t *playlist)
{
	guint len;

	g_mutex_lock (playlist->mutex);
	len = playlist->list->len;
	g_mutex_unlock (playlist->mutex);
	return len;
}

static void
xmms_playlist_remove_pos (xmms_playlist_t *playlist, guint pos)
{
	g_return_if_fail (playlist);

	/** @todo hmm, should we do currentpos++ or current-- 
	  when we remove the current entry? */
	if (playlist->currentpos >= pos) {
		playlist->currentpos--;
	}

	g_array_remove_index (playlist->list, pos);
}


static void
xmms_playlist_shuffle (xmms_playlist_t *playlist, xmms_error_t *err)
{
	guint j,i;
	guint32 cur;
	GArray *new;

	g_return_if_fail (playlist);

	if (playlist->list->len < 2) {
		return;
	}

	g_mutex_lock (playlist->mutex);

	new = g_array_new (TRUE, TRUE, sizeof (guint32));
	cur = xmms_playlist_current_entry (playlist);
	if (cur) {
		g_array_prepend_val (playlist->list, cur);
		xmms_playlist_remove_pos (playlist, cur);
	}

	for (i = 0; i < playlist->list->len; i++) {
		j = g_random_int_range (0, playlist->list->len - i);
		g_array_append_val (new, g_array_index (playlist->list, guint32, j));
		xmms_playlist_remove_pos (playlist, j);
	}

	g_array_free (playlist->list, FALSE);
	playlist->list = new;

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SHUFFLE, 0, 0);

	g_mutex_unlock (playlist->mutex);
}


/** removes entry from playlist */
gboolean 
xmms_playlist_remove (xmms_playlist_t *playlist, guint pos, xmms_error_t *err)
{
	guint32 id;

	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (id, FALSE);

	g_mutex_lock (playlist->mutex);
	id = g_array_index (playlist->list, guint32, pos);
	if (!id) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	xmms_playlist_remove_pos (playlist, pos);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_REMOVE, id, 0);

	g_mutex_unlock (playlist->mutex);

	return TRUE;
}



/** move entry in playlist */
static gboolean
xmms_playlist_move (xmms_playlist_t *playlist, guint pos, gint newpos, xmms_error_t *err)
{
	guint32 id;

	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (id, FALSE);
	g_return_val_if_fail (newpos, FALSE);

	XMMS_DBG ("Moving %d, to %d", id, newpos);

	g_mutex_lock (playlist->mutex);
	id = g_array_index (playlist->list, guint32, pos);
	if (!pos) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}
	

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_MOVE, id, newpos);

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
	
	entry = xmms_medialib_entry_new (nurl);

	if (!entry) {
		xmms_error_set (err, XMMS_ERROR_OOM, "Could not allocate memory for entry");
		return FALSE;
	}

	res = xmms_playlist_add (playlist, entry);

	return res;
}

/**
 * Add entries from medialib
 *
 * @return TRUE on success and FALSE otherwise.
 */
/*
gboolean
xmms_playlist_medialibadd (xmms_playlist_t *playlist, gchar *query, xmms_error_t *err)
{
	GList *res, *n;

	res = xmms_medialib_select_entries (query, err);

	if (xmms_error_iserror(err)) {
		return FALSE;
	}

	for (n = res; n; n = g_list_next (n)) {
		xmms_playlist_add (playlist, GPOINTER_TO_UINT (n->data));
	}
	g_list_free (res);

	return TRUE;
}
*/

/** Add entries from medialib to the playlist
 * @ingroup PlaylistClientMethods
 */


/** Adds a xmms_medialib_entry to the playlist.
 *
 *  This will append or prepend the entry according to
 *  the option.
 *  This function will wake xmms_playlist_wait.
 *  @param playlist the playlist to add the entry to.
 *  @param options should be XMMS_PLAYLIST_APPEND or XMMS_PLAYLIST_PREPEND
 *  @param file the #xmms_medialib_entry to add
 */

gboolean
xmms_playlist_add (xmms_playlist_t *playlist, xmms_medialib_entry_t file)
{

	g_return_val_if_fail (file, FALSE);

	g_mutex_lock (playlist->mutex);
	g_array_append_val (playlist->list, file);

	/** propagate the MID ! */
	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_ADD, 
				   file, 0);

	g_mutex_unlock (playlist->mutex);

	return TRUE;

}

/** Return the mediainfo for the entry
 * @ingroup PlaylistClientMethods
 */


/** Clear the playlist */
static void
xmms_playlist_clear (xmms_playlist_t *playlist, xmms_error_t *err)
{
	g_return_if_fail (playlist);

	g_mutex_lock (playlist->mutex);

	g_array_free (playlist->list, FALSE);
	playlist->list = g_array_new (TRUE, TRUE, sizeof (guint32));
	playlist->currentpos = 0;

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_CLEAR, 0, 0);
	g_mutex_unlock (playlist->mutex);

}

/** Clear the playlist
 * @ingroup PlaylistClientMethods
 */


/** Set the nextentry pointer in the playlist.
 *
 *  This will set the pointer for the next entry to be
 *  returned by xmms_playlist_advance. This function
 *  will also wake xmms_playlist_wait
 */

guint
xmms_playlist_set_current_position (xmms_playlist_t *playlist, guint pos)
{
	guint mid;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	if (pos < playlist->list->len && pos > 0) {
		playlist->currentpos = pos;
	} else {
		g_mutex_unlock (playlist->mutex);
		return 0;
	}

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SET_POS, pos, 0);

	mid = g_array_index (playlist->list, guint32, playlist->currentpos);
	g_mutex_lock (playlist->mutex);

	return mid;
}


static gint
xmms_playlist_entry_compare (gconstpointer a, gconstpointer b, gpointer data)
{
	gchar *prop = data;
	const gchar *tmpa=NULL, *tmpb=NULL;
	
	tmpa = xmms_medialib_entry_property_get (GPOINTER_TO_UINT (a), prop);
	tmpb = xmms_medialib_entry_property_get (GPOINTER_TO_UINT (b), prop);

	return g_strcasecmp (tmpa, tmpb);
}

/** Sorts the playlist by properties.
 *
 *  This will sort the list.
 *  @param property tells xmms_playlist_sort which property it
 *  should use when sorting. 
 */

static void
xmms_playlist_sort (xmms_playlist_t *playlist, gchar *property, xmms_error_t *err)
{

	g_return_if_fail (playlist);
	XMMS_DBG ("Sorting on %s", property);
	g_mutex_lock (playlist->mutex);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SORT, 0, 0);

	g_mutex_unlock (playlist->mutex);

}

/** Sort the playlist 
 * @ingroup PlaylistClientMethods
 */

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
		r = g_list_prepend (r, GUINT_TO_POINTER (g_array_index (playlist->list, guint32, i)));
	}

	r = g_list_reverse (r);
	
	g_mutex_unlock (playlist->mutex);

	return r;
}

/** List the playlist
 * @ingroup PlaylistClientMethods
 */




void
xmms_playlist_save (xmms_playlist_t *playlist, gchar *filename, xmms_error_t *err)
{
	gboolean ret;
	const gchar *mime;
	xmms_playlist_plugin_t *plsplugin;

	mime = xmms_magic_mime_from_file (filename);

	if (!mime) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "Could not determine format of output file");
		return;
	}

	plsplugin = xmms_playlist_plugin_new (mime);

	if (!plsplugin) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "Could not determine format of output file");
		return;
	}

	ret = xmms_playlist_plugin_save (plsplugin, playlist, filename);
	xmms_playlist_plugin_free (plsplugin);

	if (!ret) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "Something went wrong when writing");
	}
}


/** returns pointer to mediainfo thread. */
xmms_mediainfo_thread_t *
xmms_playlist_mediainfo_thread_get (xmms_playlist_t *playlist)
{
	g_return_val_if_fail (playlist, NULL);
	return playlist->mediainfothr;
}

/** @} */

/** Free the playlist and other memory in the xmms_playlist_t
 *
 *  This will free all entries in the list!
 */

static void
xmms_playlist_destroy (xmms_object_t *object)
{
	xmms_playlist_t *playlist = (xmms_playlist_t *)object;

	g_return_if_fail (playlist);

	g_mutex_free (playlist->mutex);

	/*
	val = xmms_config_lookup ("playlist.repeat");
	xmms_config_value_callback_remove (val, on_playlist_mode_changed);
	val = xmms_config_lookup ("playlist.stop_after_one");
	xmms_config_value_callback_remove (val, on_playlist_mode_changed);
	*/

	xmms_mediainfo_thread_stop (playlist->mediainfothr);

	g_array_free (playlist->list, FALSE);
}
