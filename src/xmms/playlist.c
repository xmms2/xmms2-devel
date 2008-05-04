/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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
#include <ctype.h>

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
static gboolean xmms_playlist_remove_unlocked (xmms_playlist_t *playlist, const gchar *plname, xmmsc_coll_t *plcoll, guint pos, xmms_error_t *err);
static gboolean xmms_playlist_move (xmms_playlist_t *playlist, gchar *plname, guint pos, guint newpos, xmms_error_t *err);
static guint xmms_playlist_set_current_position_rel (xmms_playlist_t *playlist, gint32 pos, xmms_error_t *error);
static guint xmms_playlist_set_current_position_do (xmms_playlist_t *playlist, guint32 pos, xmms_error_t *err);

static gboolean xmms_playlist_insert_url (xmms_playlist_t *playlist, gchar *plname, guint32 pos, gchar *url, xmms_error_t *error);
static gboolean xmms_playlist_insert_id (xmms_playlist_t *playlist, gchar *plname, guint32 pos, xmms_medialib_entry_t file, xmms_error_t *error);
static gboolean xmms_playlist_insert_collection (xmms_playlist_t *playlist, gchar *plname, guint32 pos, xmmsc_coll_t *coll, GList *order, xmms_error_t *error);
static void xmms_playlist_radd (xmms_playlist_t *playlist, gchar *plname, gchar *path, xmms_error_t *error);

static void xmms_playlist_load (xmms_playlist_t *, gchar *, xmms_error_t *);

static xmmsc_coll_t *xmms_playlist_get_coll (xmms_playlist_t *playlist, const gchar *plname, xmms_error_t *error);
static const gchar *xmms_playlist_canonical_name (xmms_playlist_t *playlist, const gchar *plname);
static gint xmms_playlist_coll_get_currpos (xmmsc_coll_t *plcoll);
static gint xmms_playlist_coll_get_size (xmmsc_coll_t *plcoll);

static void xmms_playlist_update_queue (xmms_playlist_t *playlist, const gchar *plname, xmmsc_coll_t *coll);
static void xmms_playlist_update_partyshuffle (xmms_playlist_t *playlist, const gchar *plname, xmmsc_coll_t *coll);

static void xmms_playlist_current_pos_msg_send (xmms_playlist_t *playlist, guint32 pos, const gchar *plname);


XMMS_CMD_DEFINE  (load, xmms_playlist_load, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE3 (insert_url, xmms_playlist_insert_url, xmms_playlist_t *, NONE, STRING, UINT32, STRING);
XMMS_CMD_DEFINE3 (insert_id, xmms_playlist_insert_id, xmms_playlist_t *, NONE, STRING, UINT32, UINT32);
XMMS_CMD_DEFINE4 (insert_coll, xmms_playlist_insert_collection, xmms_playlist_t *, NONE, STRING, UINT32, COLL, STRINGLIST);
XMMS_CMD_DEFINE  (shuffle, xmms_playlist_shuffle, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE  (remove, xmms_playlist_remove, xmms_playlist_t *, NONE, STRING, UINT32);
XMMS_CMD_DEFINE3 (move, xmms_playlist_move, xmms_playlist_t *, NONE, STRING, UINT32, UINT32);
XMMS_CMD_DEFINE  (add_url, xmms_playlist_add_url, xmms_playlist_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE  (add_id, xmms_playlist_add_id, xmms_playlist_t *, NONE, STRING, UINT32);
XMMS_CMD_DEFINE  (add_idlist, xmms_playlist_add_idlist, xmms_playlist_t *, NONE, STRING, COLL);
XMMS_CMD_DEFINE3 (add_coll, xmms_playlist_add_collection, xmms_playlist_t *, NONE, STRING, COLL, STRINGLIST);
XMMS_CMD_DEFINE  (clear, xmms_playlist_clear, xmms_playlist_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE  (sort, xmms_playlist_sort, xmms_playlist_t *, NONE, STRING, STRINGLIST);
XMMS_CMD_DEFINE  (list_entries, xmms_playlist_list_entries, xmms_playlist_t *, LIST, STRING, NONE);
XMMS_CMD_DEFINE  (current_pos, xmms_playlist_current_pos, xmms_playlist_t *, UINT32, STRING, NONE);
XMMS_CMD_DEFINE  (current_active, xmms_playlist_current_active, xmms_playlist_t *, STRING, NONE, NONE);
XMMS_CMD_DEFINE  (set_pos, xmms_playlist_set_current_position, xmms_playlist_t *, UINT32, UINT32, NONE);
XMMS_CMD_DEFINE  (set_pos_rel, xmms_playlist_set_current_position_rel, xmms_playlist_t *, UINT32, INT32, NONE);
XMMS_CMD_DEFINE  (radd, xmms_playlist_radd, xmms_playlist_t *, NONE, STRING, STRING);

#define XMMS_PLAYLIST_CHANGED_MSG(type, id, name) xmms_playlist_changed_msg_send (playlist, xmms_playlist_changed_msg_new (playlist, type, id, name))
#define XMMS_PLAYLIST_CURRPOS_MSG(pos, name) xmms_playlist_current_pos_msg_send (playlist, pos, name)


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

	gboolean update_flag;
	xmms_medialib_t *medialib;
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
on_playlist_updated (xmms_object_t *object, const gchar *plname)
{
	xmmsc_coll_t *plcoll;
	xmms_playlist_t *playlist = (xmms_playlist_t*)object;

	/* Already in an update process, quit */
	if (playlist->update_flag) {
		return;
	}

	plcoll = xmms_playlist_get_coll (playlist, plname, NULL);
	if (plcoll == NULL) {
		return;
	} else {
		/* Run the update function if appropriate */
		switch (xmmsc_coll_get_type (plcoll)) {
		case XMMS_COLLECTION_TYPE_QUEUE:
			xmms_playlist_update_queue (playlist, plname, plcoll);
			break;

		case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
			xmms_playlist_update_partyshuffle (playlist, plname, plcoll);
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
	on_playlist_updated (object, XMMS_ACTIVE_PLAYLIST);
}

static void
on_playlist_updated_chg (xmms_object_t *object, gconstpointer data,
                         gpointer udata)
{
	gchar *plname = NULL;
	xmms_object_cmd_value_t *pl_cmd_val;
	xmms_object_cmd_arg_t *val = (xmms_object_cmd_arg_t*)data;

	XMMS_DBG ("PLAYLIST: updated chg!");

	pl_cmd_val = g_tree_lookup (val->retval->value.dict, "name");
	if (pl_cmd_val != NULL) {
		plname = pl_cmd_val->value.string;
	} else {
		/* FIXME: occurs? */
		XMMS_DBG ("PLAYLIST: updated_chg, NULL playlist!");
		g_assert_not_reached ();
	}

	on_playlist_updated (object, plname);
}

static void
xmms_playlist_update_queue (xmms_playlist_t *playlist, const gchar *plname,
                            xmmsc_coll_t *coll)
{
	gint history, currpos;

	XMMS_DBG ("PLAYLIST: update-queue!");

	if (!xmms_collection_get_int_attr (coll, "history", &history)) {
		history = 0;
	}

	playlist->update_flag = TRUE;
	currpos = xmms_playlist_coll_get_currpos (coll);
	while (currpos > history) {
		xmms_playlist_remove_unlocked (playlist, plname, coll, 0, NULL);
		currpos = xmms_playlist_coll_get_currpos (coll);
	}
	playlist->update_flag = FALSE;
}

static void
xmms_playlist_update_partyshuffle (xmms_playlist_t *playlist,
                                   const gchar *plname, xmmsc_coll_t *coll)
{
	gint history, upcoming, currpos, size;
	xmmsc_coll_t *src;

	XMMS_DBG ("PLAYLIST: update-partyshuffle!");

	if (!xmms_collection_get_int_attr (coll, "history", &history)) {
		history = 0;
	}

	if (!xmms_collection_get_int_attr (coll, "upcoming", &upcoming)) {
		upcoming = XMMS_DEFAULT_PARTYSHUFFLE_UPCOMING;
	}

	playlist->update_flag = TRUE;
	currpos = xmms_playlist_coll_get_currpos (coll);
	while (currpos > history) {
		xmms_playlist_remove_unlocked (playlist, plname, coll, 0, NULL);
		currpos = xmms_playlist_coll_get_currpos (coll);
	}

	xmmsc_coll_operand_list_save (coll);
	xmmsc_coll_operand_list_first (coll);
	if (!xmmsc_coll_operand_list_entry (coll, &src)) {
		XMMS_DBG ("Cannot find party shuffle operand!");
		return;
	}
	xmmsc_coll_operand_list_restore (coll);

	size = xmms_playlist_coll_get_size (coll);
	while (size < history + 1 + upcoming) {
		xmms_medialib_entry_t randentry;
		randentry = xmms_collection_get_random_media (playlist->colldag, src);
		if (randentry == 0) {
			break;  /* No media found in the collection, give up */
		}
		/* FIXME: add_collection might yield better perf here. */
		xmms_playlist_add_entry_unlocked (playlist, plname, coll, randentry, NULL);
		size = xmms_playlist_coll_get_size (coll);
	}
	playlist->update_flag = FALSE;
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

	ret->update_flag = FALSE;

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
	                     XMMS_IPC_CMD_ADD_IDLIST,
	                     XMMS_CMD_FUNC (add_idlist));

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
	                     XMMS_IPC_CMD_RADD,
	                     XMMS_CMD_FUNC (radd));

	ret->medialib = xmms_medialib_init (ret);
	ret->mediainfordr = xmms_mediainfo_reader_start ();
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
	char *jumplist;
	xmms_error_t err;
	xmms_playlist_t *buffer = playlist;
	guint newpos;

	g_return_val_if_fail (playlist, FALSE);

	xmms_error_reset (&err);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, NULL);
	if (plcoll == NULL) {
		ret = FALSE;
	} else if ((size = xmms_playlist_coll_get_size (plcoll)) == 0) {
		if (xmmsc_coll_attribute_get (plcoll, "jumplist", &jumplist)) {
			xmms_playlist_load (buffer, jumplist, &err);
			ret = xmms_error_isok (&err);
		} else {
			ret = FALSE;
		}
	} else if (!playlist->repeat_one) {
		currpos = xmms_playlist_coll_get_currpos (plcoll);
		currpos++;

		if (currpos == size && !playlist->repeat_all &&
		    xmmsc_coll_attribute_get (plcoll, "jumplist", &jumplist)) {

			xmms_collection_set_int_attr (plcoll, "position", 0);
			XMMS_PLAYLIST_CURRPOS_MSG (0, XMMS_ACTIVE_PLAYLIST);

			xmms_playlist_load (buffer, jumplist, &err);
			ret = xmms_error_isok (&err);
		} else {
			newpos = currpos%size;
			xmms_collection_set_int_attr (plcoll, "position", newpos);
			XMMS_PLAYLIST_CURRPOS_MSG (newpos, XMMS_ACTIVE_PLAYLIST);
			ret = (currpos != size) || playlist->repeat_all;
		}
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

	plcoll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, NULL);
	if (plcoll == NULL) {
		/* FIXME: What happens? */
		g_mutex_unlock (playlist->mutex);
		return 0;
	}

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	size = xmms_playlist_coll_get_size (plcoll);

	if (currpos == -1 && (size > 0)) {
		currpos = 0;
		xmms_collection_set_int_attr (plcoll, "position", currpos);
		XMMS_PLAYLIST_CURRPOS_MSG (0, XMMS_ACTIVE_PLAYLIST);
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
xmms_playlist_current_pos (xmms_playlist_t *playlist, gchar *plname,
                           xmms_error_t *err)
{
	guint32 pos;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, 0);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		g_mutex_unlock (playlist->mutex);
		xmms_error_set (err, XMMS_ERROR_INVAL, "no such playlist");
		return 0;
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
const gchar *
xmms_playlist_current_active (xmms_playlist_t *playlist, xmms_error_t *err)
{
	const gchar *name = NULL;
	xmmsc_coll_t *active_coll;

	g_return_val_if_fail (playlist, 0);

	g_mutex_lock (playlist->mutex);

	active_coll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, err);
	if (active_coll != NULL) {
		name = xmms_collection_find_alias (playlist->colldag,
		                                   XMMS_COLLECTION_NSID_PLAYLISTS,
		                                   active_coll, XMMS_ACTIVE_PLAYLIST);
		if (name == NULL) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "active playlist not referenced!");
		}
	} else {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "no active playlist");
	}

	g_mutex_unlock (playlist->mutex);

	return name;
}


static void
xmms_playlist_load (xmms_playlist_t *playlist, gchar *name, xmms_error_t *err)
{
	xmmsc_coll_t *plcoll, *active_coll;

	if (strcmp (name, XMMS_ACTIVE_PLAYLIST) == 0) {
		xmms_error_set (err, XMMS_ERROR_INVAL, "invalid playlist to load");
		return;
	}

	active_coll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, err);
	if (active_coll == NULL) {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "no active playlist");
		return;
	}

	plcoll = xmms_playlist_get_coll (playlist, name, err);
	if (plcoll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such playlist");
		return;
	}

	if (active_coll == plcoll) {
		XMMS_DBG ("Not loading %s playlist, already active!", name);
		return;
	}

	XMMS_DBG ("Loading new playlist! %s", name);
	xmms_collection_update_pointer (playlist->colldag, XMMS_ACTIVE_PLAYLIST,
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
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such playlist");
		g_mutex_unlock (playlist->mutex);
		return;
	}

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	len = xmms_playlist_coll_get_size (plcoll);
	if (len > 1) {
		/* put current at top and exclude from shuffling */
		if (currpos != -1) {
			swap_entries (plcoll, 0, currpos);
			currpos = 0;
			xmms_collection_set_int_attr (plcoll, "position", currpos);
		}

		/* knuth <3 */
		for (i = currpos + 1; i < len; i++) {
			j = g_random_int_range (i, len);

			if (i != j) {
				swap_entries (plcoll, i, j);
			}
		}

	}

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SHUFFLE, 0, plname);
	XMMS_PLAYLIST_CURRPOS_MSG (currpos, plname);

	g_mutex_unlock (playlist->mutex);
}

static gboolean
xmms_playlist_remove_unlocked (xmms_playlist_t *playlist, const gchar *plname,
                               xmmsc_coll_t *plcoll, guint pos, xmms_error_t *err)
{
	gint currpos;
	GTree *dict;

	g_return_val_if_fail (playlist, FALSE);

	currpos = xmms_playlist_coll_get_currpos (plcoll);

	if (!xmmsc_coll_idlist_remove (plcoll, pos)) {
		if (err) xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		return FALSE;
	}

	/* decrease currentpos if removed entry was before or if it's
	 * the current entry, but only if currentpos is a valid entry.
	 */
	if (currpos != -1 && pos <= currpos) {
		currpos--;
		xmms_collection_set_int_attr (plcoll, "position", currpos);
	}

	dict = xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_REMOVE, 0, plname);
	g_tree_insert (dict, (gpointer) "position",
	               xmms_object_cmd_value_int_new (pos));
	xmms_playlist_changed_msg_send (playlist, dict);

	XMMS_PLAYLIST_CURRPOS_MSG (currpos, plname);

	return TRUE;
}

typedef struct {
	xmms_playlist_t *pls;
	xmms_medialib_entry_t entry;
} playlist_remove_info_t;

static void
remove_from_playlist (gpointer key, gpointer value, gpointer udata)
{
	playlist_remove_info_t *rminfo = (playlist_remove_info_t *) udata;
	guint32 i, val;
	gint size;
	xmmsc_coll_t *plcoll = (xmmsc_coll_t *) value;

	size = xmms_playlist_coll_get_size (plcoll);
	for (i = 0; i < size; i++) {
		if (xmmsc_coll_idlist_get_index (plcoll, i, &val) && val == rminfo->entry) {
			XMMS_DBG ("removing entry on pos %d in %s", i, (gchar *)key);
			xmms_playlist_remove_unlocked (rminfo->pls, (gchar *)key, plcoll, i, NULL);
			i--; /* reset it */
		}
	}
}



/**
 * Remove all additions of entry in the playlist
 *
 * @param playlist the playlist to remove entries from
 * @param entry the playlist entry to remove
 *
 * @sa xmms_playlist_remove
 */
gboolean
xmms_playlist_remove_by_entry (xmms_playlist_t *playlist,
                               xmms_medialib_entry_t entry)
{
	playlist_remove_info_t rminfo;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	rminfo.pls = playlist;
	rminfo.entry = entry;

	xmms_collection_foreach_in_namespace (playlist->colldag,
	                                      XMMS_COLLECTION_NSID_PLAYLISTS,
	                                      remove_from_playlist, &rminfo);

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
                    guint newpos, xmms_error_t *err)
{
	GTree *dict;
	guint32 id;
	gint currpos, size;
	gint64 ipos, inewpos;
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
	ipos = pos;
	inewpos = newpos;
	if (inewpos <= currpos && ipos > currpos)
		currpos++;
	else if (inewpos >= currpos && ipos < currpos)
		currpos--;
	else if (ipos == currpos)
		currpos = inewpos;

	xmms_collection_set_int_attr (plcoll, "position", currpos);

	xmmsc_coll_idlist_get_index (plcoll, newpos, &id);

	dict = xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_MOVE, id, plname);
	g_tree_insert (dict, (gpointer) "position",
	               xmms_object_cmd_value_int_new (pos));
	g_tree_insert (dict, (gpointer) "newposition",
	               xmms_object_cmd_value_int_new (newpos));
	xmms_playlist_changed_msg_send (playlist, dict);

	XMMS_PLAYLIST_CURRPOS_MSG (currpos, plname);

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
	xmms_medialib_session_t *session = xmms_medialib_begin_write ();

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
	GTree *dict;
	gint currpos;
	gint len;
	xmmsc_coll_t *plcoll;

	if (!xmms_medialib_check_id (file)) {
		xmms_error_set (err, XMMS_ERROR_NOENT,
		                "That is not a valid medialib id!");
		return FALSE;
	}

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
	xmmsc_coll_idlist_insert (plcoll, pos, file);

	/** propagate the MID ! */
	dict = xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_INSERT, file, plname);
	g_tree_insert (dict, (gpointer) "position",
	               xmms_object_cmd_value_int_new (pos));
	xmms_playlist_changed_msg_send (playlist, dict);

	/** update position once client is familiar with the new item. */
	currpos = xmms_playlist_coll_get_currpos (plcoll);
	if (pos <= currpos) {
		xmms_playlist_set_current_position_do (playlist, currpos + 1, err);
	}

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
		xmms_object_cmd_value_unref (val);

		res = g_list_delete_link (res, res);
	}

	/* FIXME: detect errors? */
	return TRUE;
}

/**
  * Convenient function for adding a URL to the playlist,
  * Creates a #xmms_medialib_entry_t for you and adds it
  * to the list.
  *
  * @param playlist the playlist to add it URL to.
  * @param plname the name of the playlist to modify.
  * @param nurl the URL to add
  * @param err an #xmms_error_t that should be defined upon error.
  * @return TRUE on success and FALSE otherwise.
  */
gboolean
xmms_playlist_add_url (xmms_playlist_t *playlist, gchar *plname, gchar *nurl, xmms_error_t *err)
{
	xmms_medialib_entry_t entry = 0;
	xmms_medialib_session_t *session = xmms_medialib_begin_write ();

	entry = xmms_medialib_entry_new_encoded (session, nurl, err);
	xmms_medialib_end (session);

	if (entry) {
		xmms_playlist_add_entry (playlist, plname, entry, err);
	}


	return !!entry;
}

/**
  * Convenient function for adding a directory to the playlist,
  * It will dive down the URL you feed it and recursivly add
  * all files there.
  *
  * @param playlist the playlist to add it URL to.
  * @param plname the name of the playlist to modify.
  * @param nurl the URL of an directory you want to add
  * @param err an #xmms_error_t that should be defined upon error.
  */
static void
xmms_playlist_radd (xmms_playlist_t *playlist, gchar *plname,
                    gchar *path, xmms_error_t *err)
{
	/* we actually just call the medialib function, but keep
	 * the ipc method here for not confusing users / developers
	 */
	xmms_medialib_add_recursive (playlist->medialib, plname, path, err);
}

/** Adds a xmms_medialib_entry to the playlist.
 *
 *  This will append or prepend the entry according to
 *  the option.
 *  This function will wake xmms_playlist_wait.
 *
 * @param playlist the playlist to add the entry to.
 * @param plname the name of the playlist to modify.
 * @param file the #xmms_medialib_entry_t to add
 * @param err Upon error this will be set.
 * @returns TRUE on success
 */

gboolean
xmms_playlist_add_id (xmms_playlist_t *playlist, gchar *plname,
                      xmms_medialib_entry_t file, xmms_error_t *err)
{
	if (!xmms_medialib_check_id (file)) {
		xmms_error_set (err, XMMS_ERROR_NOENT,
		                "That is not a valid medialib id!");
		return FALSE;
	}

	xmms_playlist_add_entry (playlist, plname, file, err);

	return TRUE;
}

gboolean
xmms_playlist_add_idlist (xmms_playlist_t *playlist, gchar *plname,
                          xmmsc_coll_t *coll,
                          xmms_error_t *err)
{
	uint32_t *idlist;

	for (idlist = xmmsc_coll_get_idlist (coll); *idlist; idlist++) {
		if (!xmms_medialib_check_id (*idlist)) {
			xmms_error_set (err, XMMS_ERROR_NOENT,
			                "Idlist contains invalid medialib id!");
			return FALSE;
		}
	}

	for (idlist = xmmsc_coll_get_idlist (coll); *idlist; idlist++) {
		xmms_playlist_add_entry (playlist, plname, *idlist, err);
	}

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
		xmms_object_cmd_value_unref (val);

		res = g_list_delete_link (res, res);
	}

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
	xmmsc_coll_t *plcoll;

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll != NULL) {
		xmms_playlist_add_entry_unlocked (playlist, plname, plcoll, file, err);
	}

	g_mutex_unlock (playlist->mutex);

}

/**
 * Add an entry to the playlist without locking the mutex.
 */
void
xmms_playlist_add_entry_unlocked (xmms_playlist_t *playlist,
                                  const gchar *plname,
                                  xmmsc_coll_t *plcoll,
                                  xmms_medialib_entry_t file,
                                  xmms_error_t *err)
{
	gint prev_size;
	GTree *dict;

	prev_size = xmms_playlist_coll_get_size (plcoll);
	xmmsc_coll_idlist_append (plcoll, file);

	/** propagate the MID ! */
	dict = xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_ADD, file, plname);
	g_tree_insert (dict, (gpointer) "position",
	               xmms_object_cmd_value_int_new (prev_size));
	xmms_playlist_changed_msg_send (playlist, dict);
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
		g_mutex_unlock (playlist->mutex);
		return;
	}

	xmmsc_coll_idlist_clear (plcoll);
	xmms_collection_set_int_attr (plcoll, "position", -1);

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
	char *jumplist;

	g_return_val_if_fail (playlist, FALSE);

	plcoll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, err);
	if (plcoll == NULL) {
		return 0;
	}

	size = xmms_playlist_coll_get_size (plcoll);

	if (pos == size &&
	    xmmsc_coll_attribute_get (plcoll, "jumplist", &jumplist)) {

		xmms_collection_set_int_attr (plcoll, "position", 0);
		XMMS_PLAYLIST_CURRPOS_MSG (0, XMMS_ACTIVE_PLAYLIST);

		xmms_playlist_load (playlist, jumplist, err);
		if (xmms_error_iserror (err)) {
			return 0;
		}

		plcoll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, err);
		if (plcoll == NULL) {
			return 0;
		}
	} else if (pos < size) {
		XMMS_DBG ("newpos! %d", pos);
		xmms_collection_set_int_attr (plcoll, "position", pos);
		XMMS_PLAYLIST_CURRPOS_MSG (pos, XMMS_ACTIVE_PLAYLIST);
	} else {
		xmms_error_set (err, XMMS_ERROR_INVAL,
		                "Can't set pos outside the current playlist!");
		return 0;
	}

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
	gint currpos, newpos;
	guint mid = 0;
	xmmsc_coll_t *plcoll;

	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, err);
	if (plcoll != NULL) {
		currpos = xmms_playlist_coll_get_currpos (plcoll);

		if (playlist->repeat_all) {
			newpos = (pos+currpos) % (gint)xmmsc_coll_idlist_get_size (plcoll);

			if (newpos < 0) {
				newpos += xmmsc_coll_idlist_get_size (plcoll);
			}

			mid = xmms_playlist_set_current_position_do (playlist, newpos, err);
		} else {
			if (currpos + pos >= 0) {
				mid = xmms_playlist_set_current_position_do (playlist,
				                                             currpos + pos,
				                                             err);
			} else {
				xmms_error_set (err, XMMS_ERROR_INVAL,
				                "Can't set pos outside the current playlist!");
			}
		}
	}

	g_mutex_unlock (playlist->mutex);

	return mid;
}

typedef struct {
	guint id;
	guint position;
	GList *val;  /* List of (xmms_object_cmd_value_t *) prop values */
	gboolean current;
} sortdata_t;


/**
 * Sort helper function.
 * Performs a case insesitive comparation between two entries.
 * We compare each pair of values in the list of prop values.
 */
static gint
xmms_playlist_entry_compare (gconstpointer a, gconstpointer b, gpointer user_data)
{
	GList *n1, *n2, *properties;
	xmms_object_cmd_value_t *val1, *val2;
	sortdata_t *data1 = (sortdata_t *) a;
	sortdata_t *data2 = (sortdata_t *) b;
	int s1, s2, res;
	gchar *str;

	for (n1 = data1->val, n2 = data2->val, properties = (GList *) user_data;
	     n1 && n2 && properties;
	     n1 = n1->next, n2 = n2->next, properties = properties->next) {

		str = properties->data;
		if (str[0] == '-') {
			val2 = n1->data;
			val1 = n2->data;
		} else {
			val1 = n1->data;
			val2 = n2->data;
		}

		if (!val1) {
			if (!val2)
				continue;
			else
				return -1;
		}

		if (!val2) {
			return 1;
		}

		if (val1->type == XMMS_OBJECT_CMD_ARG_STRING &&
		    val2->type == XMMS_OBJECT_CMD_ARG_STRING) {
			res = g_utf8_collate (val1->value.string,
			                      val2->value.string);
			/* keep comparing next pair if equal */
			if (res == 0)
				continue;
			else
				return res;
		}

		if ((val1->type == XMMS_OBJECT_CMD_ARG_INT32 ||
		     val1->type == XMMS_OBJECT_CMD_ARG_UINT32) &&
		    (val2->type == XMMS_OBJECT_CMD_ARG_INT32 ||
		     val2->type == XMMS_OBJECT_CMD_ARG_UINT32))
		{
			s1 = (val1->type == XMMS_OBJECT_CMD_ARG_INT32) ?
			      val1->value.int32 : val1->value.uint32;
			s2 = (val2->type == XMMS_OBJECT_CMD_ARG_INT32) ?
			      val2->value.int32 : val2->value.uint32;

			if (s1 < s2)
				return -1;
			else if (s1 > s2)
				return 1;
			else
				continue;  /* equal, compare next pair of properties */
		}

		XMMS_DBG ("Types in compare function differ to much");

		return 0;
	}

	/* all pairs matched, really equal! */
	return 0;
}

/**
 * Unwind helper function.
 * Frees the sortdata elements.
 */
static void
xmms_playlist_sorted_free (gpointer data, gpointer userdata)
{
	GList *n;
	sortdata_t *sorted = (sortdata_t *) data;

	for (n = sorted->val; n; n = n->next) {
		if (n->data) {
			xmms_object_cmd_value_unref (n->data);
		}
	}
	g_list_free (sorted->val);
	g_free (sorted);
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

	if (sorted->current) {
		size = xmmsc_coll_idlist_get_size (playlist);
		xmms_collection_set_int_attr (playlist, "position", size - 1);
	}

	xmms_playlist_sorted_free (sorted, NULL);
}

/** Sorts the playlist by properties.
 *
 *  This will sort the list.
 *  @param playlist The playlist to sort.
 *  @param properties Tells xmms_playlist_sort which properties it
 *  should use when sorting.
 *  @param err An #xmms_error_t - needed since xmms_playlist_sort is an ipc
 *  method handler.
 */

static void
xmms_playlist_sort (xmms_playlist_t *playlist, gchar *plname, GList *properties,
                    xmms_error_t *err)
{
	guint32 i;
	GList *tmp = NULL, *n;
	sortdata_t *data;
	gchar *str;
	xmms_object_cmd_value_t *val;
	xmms_medialib_session_t *session;
	gboolean list_changed = FALSE;
	xmmsc_coll_t *plcoll;
	gint currpos, size;

	g_return_if_fail (playlist);
	g_return_if_fail (properties);
	XMMS_DBG ("Sorting on %s", (char*)properties->data);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such playlist!");
		g_mutex_unlock (playlist->mutex);
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
		data->position = i;

		/* save the list of values corresponding to the list of sort props */
		data->val = NULL;
		for (n = properties; n; n = n->next) {
			str = n->data;
			if (str[0] == '-')
				str++;

			val = xmms_medialib_entry_property_get_cmd_value (session,
			                                                  data->id,
			                                                  str);

			if (val && val->type == XMMS_OBJECT_CMD_ARG_STRING) {
				str = val->value.string;
				val->value.string = g_utf8_casefold (str, strlen (str));
				g_free (str);
			}

			data->val = g_list_prepend (data->val, val);
		}
		data->val = g_list_reverse (data->val);

		data->current = (currpos == i);

		tmp = g_list_prepend (tmp, data);
	}

	xmms_medialib_end (session);

	tmp = g_list_reverse (tmp);
	tmp = g_list_sort_with_data (tmp, xmms_playlist_entry_compare, properties);

	/* check whether there was any change */
	for (i = 0, n = tmp; n; i++, n = g_list_next (n)) {
		if (((sortdata_t*)n->data)->position != i) {
			list_changed = TRUE;
			break;
		}
	}

	if (!list_changed) {
		g_list_foreach (tmp, xmms_playlist_sorted_free, NULL);
		g_list_free (tmp);
		g_mutex_unlock (playlist->mutex);
		return;
	}

	xmmsc_coll_idlist_clear (plcoll);
	g_list_foreach (tmp, xmms_playlist_sorted_unwind, plcoll);

	g_list_free (tmp);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SORT, 0, plname);
	XMMS_PLAYLIST_CURRPOS_MSG (currpos, plname);

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
	xmms_config_property_callback_remove (val, on_playlist_r_one_changed, playlist);
	val = xmms_config_lookup ("playlist.repeat_all");
	xmms_config_property_callback_remove (val, on_playlist_r_all_changed, playlist);

	xmms_object_unref (playlist->colldag);
	xmms_object_unref (playlist->mediainfordr);

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_PLAYLIST);
}


static xmmsc_coll_t *
xmms_playlist_get_coll (xmms_playlist_t *playlist, const gchar *plname,
                        xmms_error_t *error)
{
	xmmsc_coll_t *coll;
	coll = xmms_collection_get_pointer (playlist->colldag, plname,
	                                    XMMS_COLLECTION_NSID_PLAYLISTS);

	if (coll == NULL && error != NULL) {
		xmms_error_set (error, XMMS_ERROR_INVAL, "invalid playlist name");
	}

	return coll;
}

/**
 *  Retrieve the canonical name of a playlist. Assumes the playlist
 * name is valid.
 */
static const gchar *
xmms_playlist_canonical_name (xmms_playlist_t *playlist, const gchar *plname)
{
	const gchar *fullname;

	if (strcmp (plname, XMMS_ACTIVE_PLAYLIST) == 0) {
		xmmsc_coll_t *coll;
		coll = xmms_collection_get_pointer (playlist->colldag, plname,
		                                    XMMS_COLLECTION_NSID_PLAYLISTS);
		fullname = xmms_collection_find_alias (playlist->colldag,
		                                       XMMS_COLLECTION_NSID_PLAYLISTS,
		                                       coll, plname);
	} else {
		fullname = plname;
	}

	return fullname;
}

/** Get the current position in the given playlist (set to -1 if absent). */
static gint
xmms_playlist_coll_get_currpos (xmmsc_coll_t *plcoll)
{
	gint currpos;

	/* If absent, set to -1 and save it */
	if (!xmms_collection_get_int_attr (plcoll, "position", &currpos)) {
		currpos = -1;
		xmms_collection_set_int_attr (plcoll, "position", currpos);
	}

	return currpos;
}

/** Get the size of the given playlist (compute and update it if absent). */
static gint
xmms_playlist_coll_get_size (xmmsc_coll_t *plcoll)
{
	return xmmsc_coll_idlist_get_size (plcoll);
}


GTree *
xmms_playlist_changed_msg_new (xmms_playlist_t *playlist,
                               xmms_playlist_changed_actions_t type,
                               guint32 id, const gchar *plname)
{
	GTree *dict;
	xmms_object_cmd_value_t *val;
	const gchar *tmp;

	dict = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
	                        NULL, (GDestroyNotify)xmms_object_cmd_value_unref);

	val = xmms_object_cmd_value_int_new (type);
	g_tree_insert (dict, (gpointer) "type", val);

	if (id) {
		val = xmms_object_cmd_value_uint_new (id);
		g_tree_insert (dict, (gpointer) "id", val);
	}

	tmp = xmms_playlist_canonical_name (playlist, plname);
	val = xmms_object_cmd_value_str_new (tmp);
	g_tree_insert (dict, (gpointer) "name", val);

	return dict;
}

void
xmms_playlist_changed_msg_send (xmms_playlist_t *playlist, GTree *dict)
{
	xmms_object_cmd_value_t *type_cmd_val;
	xmms_object_cmd_value_t *pl_cmd_val;

	g_return_if_fail (playlist);
	g_return_if_fail (dict);

	/* If local playlist change, trigger a COLL_CHANGED signal */
	type_cmd_val = g_tree_lookup (dict, "type");
	pl_cmd_val = g_tree_lookup (dict, "name");
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

	g_tree_destroy (dict);
}

static void
xmms_playlist_current_pos_msg_send (xmms_playlist_t *playlist,
                                    guint32 pos, const gchar *plname)
{
	GTree *dict;
	xmms_object_cmd_value_t *val;
	const gchar *tmp;

	g_return_if_fail (playlist);

	dict = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
	                        NULL, (GDestroyNotify)xmms_object_cmd_value_unref);

	val = xmms_object_cmd_value_uint_new (pos);
	g_tree_insert (dict, (gpointer) "position", val);

	tmp = xmms_playlist_canonical_name (playlist, plname);
	val = xmms_object_cmd_value_str_new (tmp);
	g_tree_insert (dict, (gpointer) "name", val);

	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                    XMMS_OBJECT_CMD_ARG_DICT,
	                    dict);

	g_tree_destroy (dict);
}
