/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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
#include "xmmspriv/xmms_collection.h"
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
static void xmms_playlist_shuffle (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *err);
static void xmms_playlist_clear (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *err);
static void xmms_playlist_sort (xmms_playlist_t *playlist, gchar *plname, GList *property, xmms_error_t *err);
static GList * xmms_playlist_list_entries (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *err);
static void xmms_playlist_destroy (xmms_object_t *object);
gboolean xmms_playlist_remove (xmms_playlist_t *playlist, gchar *plname, guint pos, xmms_error_t *err);
static gboolean xmms_playlist_remove_unlocked (xmms_playlist_t *playlist, gchar *plname, xmmsc_coll_t *plcoll, guint pos, xmms_error_t *err);
static gboolean xmms_playlist_move (xmms_playlist_t *playlist, gchar *plname, guint pos, gint newpos, xmms_error_t *err);
static guint xmms_playlist_set_current_position_rel (xmms_playlist_t *playlist, gint32 pos, xmms_error_t *error);

static gboolean xmms_playlist_insert_url (xmms_playlist_t *playlist, gchar *plname, guint32 pos, gchar *url, xmms_error_t *error);
static gboolean xmms_playlist_insert_id (xmms_playlist_t *playlist, gchar *plname, guint32 pos, xmms_medialib_entry_t file, xmms_error_t *error);
static gboolean xmms_playlist_insert_collection (xmms_playlist_t *playlist, gchar *plname, guint32 pos, xmmsc_coll_t *coll, GList *order, xmms_error_t *error);

static void xmms_playlist_load (xmms_playlist_t *, gchar *, xmms_error_t *);
static void xmms_playlist_import (xmms_playlist_t *medialib, gchar *playlistname, gchar *url, xmms_error_t *error);
static gchar *xmms_playlist_export (xmms_playlist_t *medialib, gchar *playlistname, gchar *mime, xmms_error_t *error);

static xmmsc_coll_t * xmms_playlist_get_coll (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *error);
static void xmms_playlist_coll_set_int_attr (xmmsc_coll_t *plcoll, gchar *attrname, gint newval);
static gint xmms_playlist_coll_get_int_attr (xmmsc_coll_t *plcoll, gchar *attrname);
static gint xmms_playlist_coll_get_currpos (xmmsc_coll_t *plcoll);
static gint xmms_playlist_coll_get_size (xmmsc_coll_t *plcoll);

static void xmms_playlist_update_queue (xmms_playlist_t *playlist, gchar *plname, xmmsc_coll_t *coll);


XMMS_CMD_DEFINE (load, xmms_playlist_load, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE3(insert_url, xmms_playlist_insert_url, xmms_playlist_t *, NONE, STRING, UINT32, STRING);
XMMS_CMD_DEFINE3(insert_id, xmms_playlist_insert_id, xmms_playlist_t *, NONE, STRING, UINT32, UINT32);
XMMS_CMD_DEFINE4(insert_coll, xmms_playlist_insert_collection, xmms_playlist_t *, NONE, STRING, UINT32, COLL, STRINGLIST);
XMMS_CMD_DEFINE (shuffle, xmms_playlist_shuffle, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (remove, xmms_playlist_remove, xmms_playlist_t *, NONE, STRING, UINT32);
XMMS_CMD_DEFINE3(move, xmms_playlist_move, xmms_playlist_t *, NONE, STRING, UINT32, INT32);
XMMS_CMD_DEFINE (add_url, xmms_playlist_add_url, xmms_playlist_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE (add_id, xmms_playlist_add_id, xmms_playlist_t *, NONE, STRING, UINT32);
XMMS_CMD_DEFINE3(add_coll, xmms_playlist_add_collection, xmms_playlist_t *, NONE, STRING, COLL, STRINGLIST);
XMMS_CMD_DEFINE (clear, xmms_playlist_clear, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (sort, xmms_playlist_sort, xmms_playlist_t *, NONE, STRING, STRINGLIST);
XMMS_CMD_DEFINE (list_entries, xmms_playlist_list_entries, xmms_playlist_t *, LIST, STRING, NONE);
XMMS_CMD_DEFINE (current_pos, xmms_playlist_current_pos, xmms_playlist_t *, UINT32, NONE, NONE);
XMMS_CMD_DEFINE (current_active, xmms_playlist_current_active, xmms_playlist_t *, STRING, NONE, NONE);
XMMS_CMD_DEFINE (set_pos, xmms_playlist_set_current_position, xmms_playlist_t *, UINT32, UINT32, NONE);
XMMS_CMD_DEFINE (set_pos_rel, xmms_playlist_set_current_position_rel, xmms_playlist_t *, UINT32, INT32, NONE);
XMMS_CMD_DEFINE (import, xmms_playlist_import, xmms_playlist_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE (export, xmms_playlist_export, xmms_playlist_t *, STRING, STRING, STRING);


#define XMMS_PLAYLIST_CHANGED_MSG(type, id, name) xmms_playlist_changed_msg_send (playlist, xmms_playlist_changed_msg_new (type, id, name))


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

	/* playlists are in the collection DAG */
	xmms_coll_dag_t *colldag;

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


static void
on_playlist_updated (xmms_object_t *object, gchar *plname)
{
	xmmsc_coll_t *plcoll;
	xmms_playlist_t *playlist = object;

	plcoll = xmms_playlist_get_coll (playlist, plname, NULL);
	if (plcoll == NULL) {
		return;
	}
	else {
		/* Run the update function if appropriate */
		switch (xmmsc_coll_get_type (plcoll)) {
		case XMMS_COLLECTION_TYPE_QUEUE:
			xmms_playlist_update_queue (playlist, plname, plcoll);
			break;

		default:
			break;
		}
	}
}

static void
on_playlist_updated_pos (xmms_object_t *object, gconstpointer data,
                         gpointer udata)
{
	XMMS_DBG ("PLAYLIST: updated pos!");
	on_playlist_updated (object, "_active");
}

static void
on_playlist_updated_chg (xmms_object_t *object, gconstpointer data,
                         gpointer udata)
{
	gchar *plname = NULL;
	xmms_object_cmd_value_t *pl_cmd_val;
	xmms_object_cmd_arg_t *val = (xmms_object_cmd_arg_t*)data;

	XMMS_DBG ("PLAYLIST: updated chg!");

	pl_cmd_val = g_hash_table_lookup (val->retval->value.dict, "name");
	if (pl_cmd_val != NULL) {
		plname = pl_cmd_val->value.string;
	}
	else {
		/* FIXME: occurs? */
		XMMS_DBG ("PLAYLIST: updated_chg, NULL playlist!");
		g_assert_not_reached ();
	}

	on_playlist_updated (object, plname);
}

static void
xmms_playlist_update_queue (xmms_playlist_t *playlist, gchar *plname,
                            xmmsc_coll_t *coll)
{
	gint history, currpos;

	XMMS_DBG ("PLAYLIST: update-queue!");

	history = xmms_playlist_coll_get_int_attr (coll, "history");
	if (history == -1) {
		history = 0;
	}

	currpos = xmms_playlist_coll_get_currpos (coll);
	while (currpos > history) {
		xmms_playlist_remove_unlocked (playlist, plname, coll, 0, NULL);
		currpos = xmms_playlist_coll_get_currpos (coll);
	}
}


/**
 * Initializes a new xmms_playlist_t.
 */
xmms_playlist_t *
xmms_playlist_init (void)
{
	xmms_playlist_t *ret;
	xmms_config_property_t *val;

	ret = xmms_object_new (xmms_playlist_t, xmms_playlist_destroy);
	ret->mutex = g_mutex_new ();

	xmms_ipc_object_register (XMMS_IPC_OBJECT_PLAYLIST, XMMS_OBJECT (ret));

	xmms_ipc_broadcast_register (XMMS_OBJECT (ret),
	                             XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
	xmms_ipc_broadcast_register (XMMS_OBJECT (ret),
	                             XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);
	xmms_ipc_broadcast_register (XMMS_OBJECT (ret),
	                             XMMS_IPC_SIGNAL_PLAYLIST_LOADED);

	val = xmms_config_property_register ("playlist.repeat_one", "0",
	                                     on_playlist_r_one_changed, ret);
	ret->repeat_one = xmms_config_property_get_int (val);
	
	val = xmms_config_property_register ("playlist.repeat_all", "0",
	                                  on_playlist_r_all_changed, ret);
	ret->repeat_all = xmms_config_property_get_int (val);


	xmms_object_connect (XMMS_OBJECT (ret),
	                     XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	                     on_playlist_updated_chg, ret);

	xmms_object_connect (XMMS_OBJECT (ret),
	                     XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                     on_playlist_updated_pos, ret);


	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_CURRENT_POS, 
			     XMMS_CMD_FUNC (current_pos));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_CURRENT_ACTIVE, 
			     XMMS_CMD_FUNC (current_active));

	xmms_object_cmd_add (XMMS_OBJECT (ret),
			     XMMS_IPC_CMD_LOAD,
			     XMMS_CMD_FUNC (load));

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
			     XMMS_IPC_CMD_ADD_URL, 
			     XMMS_CMD_FUNC (add_url));
	
	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_ADD_ID, 
			     XMMS_CMD_FUNC (add_id));

	xmms_object_cmd_add (XMMS_OBJECT (ret),
			     XMMS_IPC_CMD_ADD_COLL,
			     XMMS_CMD_FUNC (add_coll));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_REMOVE_ENTRY, 
			     XMMS_CMD_FUNC (remove));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_MOVE_ENTRY, 
			     XMMS_CMD_FUNC (move));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_CLEAR, 
			     XMMS_CMD_FUNC (clear));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_SORT, 
			     XMMS_CMD_FUNC (sort));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_LIST, 
			     XMMS_CMD_FUNC (list_entries));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_INSERT_URL, 
			     XMMS_CMD_FUNC (insert_url));

	xmms_object_cmd_add (XMMS_OBJECT (ret), 
			     XMMS_IPC_CMD_INSERT_ID, 
			     XMMS_CMD_FUNC (insert_id));

	xmms_object_cmd_add (XMMS_OBJECT (ret),
			     XMMS_IPC_CMD_INSERT_COLL,
			     XMMS_CMD_FUNC (insert_coll));

	xmms_object_cmd_add (XMMS_OBJECT (ret),
			     XMMS_IPC_CMD_IMPORT,
			     XMMS_CMD_FUNC (import));

	xmms_object_cmd_add (XMMS_OBJECT (ret),
			     XMMS_IPC_CMD_EXPORT,
			     XMMS_CMD_FUNC (export));

	xmms_medialib_init (ret);

	ret->mediainfordr = xmms_mediainfo_reader_start (ret);
	ret->colldag = xmms_collection_init (ret);

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
	gint size, currpos;
	gboolean ret = TRUE;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, "_active", NULL);
	if (plcoll == NULL) {
		ret = FALSE;
	} else if ((size = xmms_playlist_coll_get_size (plcoll)) == 0) {
		ret = FALSE;
	} else if (!playlist->repeat_one) {
		currpos = xmms_playlist_coll_get_currpos (plcoll);
		currpos++;
		currpos %= size;
		xmms_playlist_coll_set_int_attr (plcoll, "position", currpos);
		ret = (currpos != 0) || playlist->repeat_all;
		xmms_object_emit_f (XMMS_OBJECT (playlist),
		                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
		                    XMMS_OBJECT_CMD_ARG_UINT32,
		                    currpos);
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
	gint size, currpos;
	xmmsc_coll_t *plcoll;
	xmms_medialib_entry_t ent = 0;

	g_return_val_if_fail (playlist, 0);
	
	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, "_active", NULL);
	if (plcoll == NULL) {
		/* FIXME: What happens? */
		g_mutex_unlock (playlist->mutex);
		return 0;
	}

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	size = xmms_playlist_coll_get_size (plcoll);

	if (currpos == -1 && (size > 0)) {
		currpos = 0;
		xmms_playlist_coll_set_int_attr (plcoll, "position", currpos);
		xmms_object_emit_f (XMMS_OBJECT (playlist),
				    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
				    XMMS_OBJECT_CMD_ARG_UINT32, 0);
	}

	if (currpos < size) {
		guint *idlist;
		idlist = xmmsc_coll_get_idlist (plcoll);
		ent = idlist[currpos];
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
xmms_playlist_current_pos (xmms_playlist_t *playlist, xmms_error_t *err)
{
	guint32 pos;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, 0);
	
	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, "_active", err);
	if (plcoll == NULL) {
		g_mutex_unlock (playlist->mutex);
		return -1;
	}

	pos = xmms_playlist_coll_get_currpos (plcoll);
	if (pos == -1) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "no current entry");
	}

	g_mutex_unlock (playlist->mutex);

	return pos;
}

/**
 * Retrieve the name of the currently active playlist.
 *
 */
gchar *
xmms_playlist_current_active (xmms_playlist_t *playlist, xmms_error_t *err)
{
	gchar *name = NULL;
	xmmsc_coll_t *active_coll;

	g_return_val_if_fail (playlist, 0);
	
	g_mutex_lock (playlist->mutex);

	active_coll = xmms_playlist_get_coll (playlist, "_active", err);
	if (active_coll != NULL) {
		name = xmms_collection_find_alias (playlist->colldag,
		                                   XMMS_COLLECTION_NSID_PLAYLISTS,
		                                   active_coll, "_active");
		if (name == NULL) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "active playlist not referenced!");
		}
	}
	else {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "no active playlist");
	}

	g_mutex_unlock (playlist->mutex);

	return name;
}


static void
xmms_playlist_load (xmms_playlist_t *playlist, gchar *name, xmms_error_t *err)
{
	xmmsc_coll_t *plcoll;

	if (strcmp (name, "_active") == 0) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid playlist to load");
		return;
	}

	plcoll = xmms_playlist_get_coll (playlist, name, err);
	if (plcoll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such playlist");
		return;
	}

	xmms_collection_update_pointer (playlist->colldag, "_active", 
	                                XMMS_COLLECTION_NSID_PLAYLISTS, plcoll);

	xmms_object_emit_f (XMMS_OBJECT (playlist), 
	                    XMMS_IPC_SIGNAL_PLAYLIST_LOADED, 
	                    XMMS_OBJECT_CMD_ARG_STRING,
	                    name);
}

static inline void
swap_entries (xmmsc_coll_t *coll, gint i, gint j)
{
	guint32 tmp, tmp2;

	xmmsc_coll_idlist_get_index (coll, i, &tmp);
	xmmsc_coll_idlist_get_index (coll, j, &tmp2);

	xmmsc_coll_idlist_set_index (coll, i, tmp2);
	xmmsc_coll_idlist_set_index (coll, j, tmp);
}


/**
 * Shuffle the playlist.
 *
 */
static void
xmms_playlist_shuffle (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *err)
{
	guint j,i;
	gint len, currpos;
	xmmsc_coll_t *plcoll;

	g_return_if_fail (playlist);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		/* FIXME: happens? */
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such playlist");
		return;
	}

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	len = xmms_playlist_coll_get_size (plcoll);
	if (len > 1) {
		/* put current at top and exclude from shuffling */
		if (currpos != -1) {
			swap_entries (plcoll, 0, currpos);
			currpos = 0;
			xmms_playlist_coll_set_int_attr (plcoll, "position", currpos);
		}

		/* knuth <3 */
		for (i = 1; i < len; i++) {
			j = g_random_int_range (i, len);

			if (i != j) {
				swap_entries (plcoll, i, j);
			}
		}

	}

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SHUFFLE, 0, plname);
	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                    XMMS_OBJECT_CMD_ARG_UINT32,
	                    currpos);

	g_mutex_unlock (playlist->mutex);
}

static gboolean
xmms_playlist_remove_unlocked (xmms_playlist_t *playlist, gchar *plname,
                               xmmsc_coll_t *plcoll, guint pos, xmms_error_t *err)
{
	gint currpos, size;
	GHashTable *dict;

	g_return_val_if_fail (playlist, FALSE);

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	size = xmms_playlist_coll_get_size (plcoll);

	if (pos >= size) {
		if (err) xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	xmmsc_coll_idlist_remove (plcoll, pos);
	xmms_playlist_coll_set_int_attr (plcoll, "size", size - 1);

	/* decrease currentpos if removed entry was before or if it's
	 * the current entry, but only if currentpos is a valid entry.
	 */
	if (currpos != -1 && pos <= currpos) {
		currpos--;
		xmms_playlist_coll_set_int_attr (plcoll, "position", currpos);
	}

	dict = xmms_playlist_changed_msg_new (XMMS_PLAYLIST_CHANGED_REMOVE, 0, plname);
	g_hash_table_insert (dict, "position", xmms_object_cmd_value_int_new (pos));
	xmms_playlist_changed_msg_send (playlist, dict);
	
	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                    XMMS_OBJECT_CMD_ARG_UINT32,
	                    currpos);

	return TRUE;
}

/**
 * Remove all additions of #entry in the playlist
 * @sa xmms_playlist_remove
 */
gboolean
xmms_playlist_remove_by_entry (xmms_playlist_t *playlist, gchar *plname,
                               xmms_medialib_entry_t entry)
{
	guint32 i, val;
	gint size;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, NULL);
	if (plcoll == NULL) {
		/* FIXME: happens ? */
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	size = xmms_playlist_coll_get_size (plcoll);
	for (i = 0; i < size; i++) {
		if (xmmsc_coll_idlist_get_index (plcoll, i, &val) && val == entry) {
			XMMS_DBG ("removing entry on pos %d", i);
			xmms_playlist_remove_unlocked (playlist, plname, plcoll, i, NULL);
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
xmms_playlist_remove (xmms_playlist_t *playlist, gchar *plname, guint pos,
                      xmms_error_t *err)
{
	gboolean ret = FALSE;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);
	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll != NULL) {
		ret = xmms_playlist_remove_unlocked (playlist, plname, plcoll, pos, err);
	}
	g_mutex_unlock (playlist->mutex);
	return ret;
}


/**
 * Move an entry in playlist
 *
 */
static gboolean
xmms_playlist_move (xmms_playlist_t *playlist, gchar *plname, guint pos,
                    gint newpos, xmms_error_t *err)
{
	GHashTable *dict;
	guint32 id;
	gint currpos, size;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, FALSE);

	XMMS_DBG ("Moving %d, to %d", pos, newpos);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		/* FIXME: happens ? */
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	size = xmms_playlist_coll_get_size (plcoll);
	
	if (size == 0 || newpos > (size - 1)) {
		xmms_error_set (err, XMMS_ERROR_NOENT,
		                "Cannot move entry outside playlist");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	if (!xmmsc_coll_idlist_move (plcoll, pos, newpos)) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	/* Update the current position pointer */
	if (newpos <= currpos && pos > currpos)
		currpos++;
	else if (newpos >= currpos && pos < currpos)
		currpos--;
	else if (pos == currpos)
		currpos = newpos;

	xmms_playlist_coll_set_int_attr (plcoll, "position", currpos);

	xmmsc_coll_idlist_get_index (plcoll, newpos, &id);

	dict = xmms_playlist_changed_msg_new (XMMS_PLAYLIST_CHANGED_MOVE, id, plname);
	g_hash_table_insert (dict, "position", xmms_object_cmd_value_int_new (pos));
	g_hash_table_insert (dict, "newposition", xmms_object_cmd_value_int_new (newpos));
	xmms_playlist_changed_msg_send (playlist, dict);

	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                    XMMS_OBJECT_CMD_ARG_UINT32,
	                    currpos);

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
xmms_playlist_insert_url (xmms_playlist_t *playlist, gchar *plname, guint32 pos,
                          gchar *url, xmms_error_t *err)
{
	xmms_medialib_entry_t entry = 0;
	xmms_medialib_session_t *session = xmms_medialib_begin_write();

	entry = xmms_medialib_entry_new_encoded (session, url, err);
	xmms_medialib_end (session);

	if (!entry) {
		return FALSE;
	}

	return xmms_playlist_insert_id (playlist, plname, pos, entry, err);
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
xmms_playlist_insert_id (xmms_playlist_t *playlist, gchar *plname, guint32 pos,
                         xmms_medialib_entry_t file, xmms_error_t *err)
{
	GHashTable *dict;
	gint len;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (file, FALSE);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		/* FIXME: happens ? */
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}

	len = xmms_playlist_coll_get_size (plcoll);
	if (pos > len || pos < 0) {
		xmms_error_set (err, XMMS_ERROR_GENERIC,
		                "Could not insert entry outside of playlist!");
		g_mutex_unlock (playlist->mutex);
		return FALSE;
	}
	xmmsc_coll_idlist_insert (plcoll, file, pos);
	xmms_playlist_coll_set_int_attr (plcoll, "size", len + 1);

	/** propagate the MID ! */
	dict = xmms_playlist_changed_msg_new (XMMS_PLAYLIST_CHANGED_INSERT, file, plname);
	g_hash_table_insert (dict, "position", xmms_object_cmd_value_int_new (pos));
	xmms_playlist_changed_msg_send (playlist, dict);
	
	g_mutex_unlock (playlist->mutex);
	return TRUE;
}

gboolean
xmms_playlist_insert_collection (xmms_playlist_t *playlist, gchar *plname,
                                 guint32 pos, xmmsc_coll_t *coll, GList *order,
                                 xmms_error_t *err)
{
	GList *res;

	res = xmms_collection_query_ids (playlist->colldag, coll, 0, 0, order, err);

	while (res) {
		xmms_object_cmd_value_t *val = (xmms_object_cmd_value_t*)res->data;
		xmms_playlist_insert_id (playlist, plname, pos, val->value.int32, err);
		g_free (res->data);
		res = res->next;
	}

	g_list_free (res);

	/* FIXME: detect errors? */
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
xmms_playlist_add_url (xmms_playlist_t *playlist, gchar *plname, gchar *nurl, xmms_error_t *err)
{
	xmms_medialib_entry_t entry = 0;
	xmms_medialib_session_t *session = xmms_medialib_begin_write();
	
	entry = xmms_medialib_entry_new_encoded (session, nurl, err);

	if (entry) {
		xmms_playlist_add_entry (playlist, plname, entry, err);
	}

	xmms_medialib_end (session);

	return !!entry;
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
xmms_playlist_add_id (xmms_playlist_t *playlist, gchar *plname,
                      xmms_medialib_entry_t file, xmms_error_t *err)
{
	g_return_val_if_fail (file, FALSE);

	if (!xmms_medialib_check_id (file)) {
		if (err) {
			/* we can be called internaly also! */
			xmms_error_set (err, XMMS_ERROR_NOENT, 
			                "That is not a valid medialib id!");
		}
		return FALSE;
	}

	xmms_playlist_add_entry (playlist, plname, file, err);

	return TRUE;
}

gboolean
xmms_playlist_add_collection (xmms_playlist_t *playlist, gchar *plname,
                              xmmsc_coll_t *coll, GList *order,
                              xmms_error_t *err)
{
	GList *res;

	res = xmms_collection_query_ids (playlist->colldag, coll, 0, 0, order, err);

	while (res) {
		xmms_object_cmd_value_t *val = (xmms_object_cmd_value_t*)res->data;
		xmms_playlist_add_entry (playlist, plname, val->value.int32, err);
		g_free (res->data);
		res = res->next;
	}

	g_list_free (res);

	/* FIXME: detect errors? */
	return TRUE;
}

/**
 * Add an entry to the playlist without validating it.
 *
 * @internal
 */
void
xmms_playlist_add_entry (xmms_playlist_t *playlist, gchar *plname,
                         xmms_medialib_entry_t file, xmms_error_t *err)
{
	gint prev_size;
	GHashTable *dict;
	xmmsc_coll_t *plcoll;

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		/* FIXME: happens ? */
		g_mutex_unlock (playlist->mutex);
		return;
	}

	prev_size = xmms_playlist_coll_get_size (plcoll);
	xmmsc_coll_idlist_append (plcoll, file);
	xmms_playlist_coll_set_int_attr (plcoll, "size", prev_size + 1);

	/** propagate the MID ! */
	dict = xmms_playlist_changed_msg_new (XMMS_PLAYLIST_CHANGED_ADD, file, plname);
	g_hash_table_insert (dict, "position", xmms_object_cmd_value_int_new (prev_size));
	xmms_playlist_changed_msg_send (playlist, dict);

	g_mutex_unlock (playlist->mutex);

}

/** Clear the playlist */
static void
xmms_playlist_clear (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *err)
{
	xmmsc_coll_t *plcoll;

	g_return_if_fail (playlist);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		return;
	}

	xmmsc_coll_idlist_clear (plcoll);
	xmms_playlist_coll_set_int_attr (plcoll, "position", -1);
	xmms_playlist_coll_set_int_attr (plcoll, "size", 0);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_CLEAR, 0, plname);
	g_mutex_unlock (playlist->mutex);

}


/** Set the nextentry pointer in the playlist.
 *
 *  This will set the pointer for the next entry to be
 *  returned by xmms_playlist_advance. This function
 *  will also wake xmms_playlist_wait
 */

static guint
xmms_playlist_set_current_position_do (xmms_playlist_t *playlist, guint32 pos,
                                       xmms_error_t *err)
{
	gint size;
	guint mid;
	guint *idlist;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, FALSE);

	plcoll = xmms_playlist_get_coll (playlist, "_active", err);
	if (plcoll == NULL) {
		return 0;
	}

	size = xmms_playlist_coll_get_size (plcoll);

	if (pos >= size) {
		xmms_error_set (err, XMMS_ERROR_INVAL,
		                "Can't set pos outside the current playlist!");
		return 0;
	}

	XMMS_DBG ("newpos! %d", pos);
	xmms_playlist_coll_set_int_attr (plcoll, "position", pos);

	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                    XMMS_OBJECT_CMD_ARG_UINT32,
	                    pos);

	idlist = xmmsc_coll_get_idlist (plcoll);
	mid = idlist[pos];

	return mid;
}

guint
xmms_playlist_set_current_position (xmms_playlist_t *playlist, guint32 pos,
                                    xmms_error_t *err)
{
	guint mid;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);
	mid = xmms_playlist_set_current_position_do (playlist, pos, err);
	g_mutex_unlock (playlist->mutex);

	return mid;
}

static guint
xmms_playlist_set_current_position_rel (xmms_playlist_t *playlist, gint32 pos,
                                        xmms_error_t *err)
{
	gint currpos;
	guint mid = 0;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, "_active", err);
	if (plcoll != NULL) {
		currpos = xmms_playlist_coll_get_currpos (plcoll);

		if (currpos + pos >= 0)
			mid = xmms_playlist_set_current_position_do (playlist, currpos + pos, err);
	}

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
	gint size;
	sortdata_t *sorted = (sortdata_t *) data;
	xmmsc_coll_t *playlist = (xmmsc_coll_t *)userdata;

	xmmsc_coll_idlist_append (playlist, sorted->id);

	if (sorted->val) {
		xmms_object_cmd_value_free (sorted->val);
	}

	if (sorted->current) {
		size = xmms_playlist_coll_get_size (playlist);
		xmms_playlist_coll_set_int_attr (playlist, "position", size - 1);
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
xmms_playlist_sort (xmms_playlist_t *playlist, gchar *plname, GList *property,
                    xmms_error_t *err)
{
	/* FIXME: handle properties as a list */
	/* FIXME: Or use coll sorting ? */ 

	guint32 i;
	GList *tmp = NULL, *sorted, *l, *l2;
	sortdata_t *data;
	gchar *str;
	xmms_medialib_session_t *session;
	gboolean list_changed = FALSE;
	xmmsc_coll_t *plcoll;
	gint currpos, size;

	g_return_if_fail (playlist);
	g_return_if_fail (property);
	XMMS_DBG ("Sorting on %s", (char*)property->data);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such playlist!");
		return;
	}

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	size = xmms_playlist_coll_get_size (plcoll);

	/* check whether we need to do any sorting at all */
	if (size < 2) {
		g_mutex_unlock (playlist->mutex);
		return;
	}

	session = xmms_medialib_begin ();

	for (i = 0; i < size; i++) {
		data = g_new (sortdata_t, 1);

		xmmsc_coll_idlist_get_index (plcoll, i, &data->id);
		data->val =
			xmms_medialib_entry_property_get_cmd_value (session,
			                                            data->id,
			                                            (char*)property->data);

		if (data->val && data->val->type == XMMS_OBJECT_CMD_ARG_STRING) {
			str = data->val->value.string;
			data->val->value.string = g_utf8_casefold (str, strlen (str));
			g_free (str);
		}

		data->current = (currpos == i);

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

	xmmsc_coll_idlist_clear (plcoll);
	g_list_foreach (sorted, xmms_playlist_sorted_unwind, plcoll);

	g_list_free (sorted);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SORT, 0, plname);

	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                    XMMS_OBJECT_CMD_ARG_UINT32,
	                    currpos);

	g_mutex_unlock (playlist->mutex);
}


/** List a playlist */
static GList *
xmms_playlist_list_entries (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *err)
{
	GList *entries = NULL;
	xmmsc_coll_t *plcoll;
	guint *idlist;
	gint i;

	g_return_val_if_fail (playlist, NULL);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		g_mutex_unlock (playlist->mutex);
		return NULL;
	}

	idlist = xmmsc_coll_get_idlist (plcoll);

	for (i = 0; idlist[i] != 0; i++) {
		entries = g_list_prepend (entries, xmms_object_cmd_value_uint_new (idlist[i]));
	}

	g_mutex_unlock (playlist->mutex);

	entries = g_list_reverse (entries);

	return entries;
}


static void
xmms_playlist_import (xmms_playlist_t *playlist, gchar *name,
                      gchar *url, xmms_error_t *error)
{
	/* FIXME: Real code */
/*
	gint playlist_id;
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin_write ();
	entry = xmms_medialib_entry_new (session, url, error);
	if (!entry) {
		xmms_medialib_end (session);
		return;
	}

	playlist_id = get_playlist_id (session, name);

	playlist_id = prepare_playlist (session, playlist_id, name, -1);
	if (!playlist_id) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
						"Couldn't prepare playlist");
		xmms_medialib_end (session);

		return;
	}

	xmms_medialib_end (session);

/\*	if (!xmms_playlist_plugin_import (playlist_id, entry)) {*\/
	if (FALSE) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Could not import playlist!");
		return;
	}

	xmms_mediainfo_reader_wakeup (xmms_playlist_mediainfo_reader_get (medialib->playlist));
*/
}


static gchar *
xmms_playlist_export (xmms_playlist_t *playlist, gchar *playlistname, 
                      gchar *mime, xmms_error_t *error)
{
	/* FIXME: Real code */
/*
	GString *str;
	GList *entries = NULL;
	guint *list;
	gint ret;
	gint i;
	gint plsid;
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin ();
	
	plsid = get_playlist_id (session, playlistname);
	if (!plsid) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "No such playlist!");
		xmms_medialib_end (session);
		return NULL;
	}

	ret = xmms_sqlite_query_array (session->sql, get_playlist_entries_cb, &entries,
								   "select entry from PlaylistEntries "
								   "where playlist_id = %u "
								   "order by pos", plsid);

	xmms_medialib_end (session);

	if (!ret) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Failed to list entries!");
		return NULL;
	}

	entries = g_list_reverse (entries);

	list = g_malloc0 (sizeof (guint) * g_list_length (entries) + 1);
	i = 0;
	while (entries) {
		gchar *e = entries->data;

		if (g_strncasecmp (e, "mlib://", 7) == 0) {
			list[i] = atoi (e+7);
			i++;
		}

		g_free (e);
		entries = g_list_delete_link (entries, entries);
	}

	/\*
	str = xmms_playlist_plugin_save (mime, list);
	*\/
	str = NULL;
	if (!str) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Failed to generate playlist!");
		return NULL;
	}

	return str->str;
*/
	return NULL;
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

	g_mutex_free (playlist->mutex);

	val = xmms_config_lookup ("playlist.repeat_one");
	xmms_config_property_callback_remove (val, on_playlist_r_one_changed);
	val = xmms_config_lookup ("playlist.repeat_all");
	xmms_config_property_callback_remove (val, on_playlist_r_all_changed);

	xmms_object_unref (playlist->mediainfordr);

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_PLAYLIST);
}


static xmmsc_coll_t *
xmms_playlist_get_coll (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *error)
{
	xmmsc_coll_t *coll;
	coll = xmms_collection_get_pointer (playlist->colldag, plname,
	                                    XMMS_COLLECTION_NSID_PLAYLISTS);

	if (coll == NULL && error != NULL) {
		xmms_error_set (error, XMMS_ERROR_INVAL, "invalid playlist name");
	}

	return coll;
}

static void
xmms_playlist_coll_set_int_attr (xmmsc_coll_t *plcoll, gchar *attrname, gint newval)
{
	gchar *str;

	/* FIXME: Check for (soft) overflow */
	str = g_new (char, XMMS_MAX_INT_ATTRIBUTE_LEN + 1);
	g_snprintf (str, XMMS_MAX_INT_ATTRIBUTE_LEN, "%d", newval);

	xmmsc_coll_attribute_set (plcoll, attrname, str);
	g_free (str);
}

static gint
xmms_playlist_coll_get_int_attr (xmmsc_coll_t *plcoll, gchar *attrname)
{
	gint val;
	gchar *str;
	gchar *endptr;

	if (!xmmsc_coll_attribute_get (plcoll, attrname, &str)) {
		val = -1;
	}
	else {
		val = strtol (str, &endptr, 10);
		if (*endptr != '\0') {
			/* FIXME: Invalid characters in the string ! */
		}
	}

	return val;
}

/** Get the current position in the given playlist (set to -1 if absent). */
static gint
xmms_playlist_coll_get_currpos (xmmsc_coll_t *plcoll)
{
	gint currpos;

	currpos = xmms_playlist_coll_get_int_attr (plcoll, "position");

	if (currpos == -1) {
		xmms_playlist_coll_set_int_attr (plcoll, "position", -1);
	}

	return currpos;
}

/** Get the size of the given playlist (compute and update it if absent). */
static gint
xmms_playlist_coll_get_size (xmmsc_coll_t *plcoll)
{
	gint size;

	size = xmms_playlist_coll_get_int_attr (plcoll, "size");

	if (size == -1) {
		gint i;
		guint *idlist;

		size = 0;
		idlist = xmmsc_coll_get_idlist (plcoll);
		for (i = 0; idlist[i] != 0; i++) {
			size++;
		}

		xmms_playlist_coll_set_int_attr (plcoll, "size", size);
	}

	return size;
}


GHashTable *
xmms_playlist_changed_msg_new (xmms_playlist_changed_actions_t type,
                               guint32 id, gchar *plname)
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
	val = xmms_object_cmd_value_str_new (plname);
	g_hash_table_insert (dict, "name", val);

	return dict;
}

void
xmms_playlist_changed_msg_send (xmms_playlist_t *playlist, GHashTable *dict)
{
	xmms_object_cmd_value_t *type_cmd_val;
	xmms_object_cmd_value_t *pl_cmd_val;

	g_return_if_fail (playlist);
	g_return_if_fail (dict);

	/* If local playlist change, trigger a COLL_CHANGED signal */
	type_cmd_val = g_hash_table_lookup (dict, "type");
	pl_cmd_val = g_hash_table_lookup (dict, "name");
	if (type_cmd_val != NULL &&
	    type_cmd_val->value.int32 != XMMS_PLAYLIST_CHANGED_UPDATE &&
	    pl_cmd_val != NULL) {
		XMMS_COLLECTION_PLAYLIST_CHANGED_MSG (playlist->colldag,
		                                      pl_cmd_val->value.string);
	}

	xmms_object_emit_f (XMMS_OBJECT (playlist),
			    XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
			    XMMS_OBJECT_CMD_ARG_DICT,
			    dict);

	g_hash_table_destroy (dict);
}
