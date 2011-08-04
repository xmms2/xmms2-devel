/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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
static void xmms_playlist_client_shuffle (xmms_playlist_t *playlist, const gchar *plname, xmms_error_t *err);
static void xmms_playlist_client_clear (xmms_playlist_t *playlist, const gchar *plname, xmms_error_t *err);
static void xmms_playlist_client_sort (xmms_playlist_t *playlist, const gchar *plname, xmmsv_t *property, xmms_error_t *err);
static GList * xmms_playlist_client_list_entries (xmms_playlist_t *playlist, const gchar *plname, xmms_error_t *err);
static gchar *xmms_playlist_client_current_active (xmms_playlist_t *playlist, xmms_error_t *err);
static void xmms_playlist_destroy (xmms_object_t *object);

static void xmms_playlist_client_add_id (xmms_playlist_t *playlist, const gchar *plname, xmms_medialib_entry_t file, xmms_error_t *error);
static void xmms_playlist_client_add_url (xmms_playlist_t *playlist, const gchar *plname, const gchar *nurl, xmms_error_t *err);
static void xmms_playlist_client_add_idlist (xmms_playlist_t *playlist, const gchar *plname, xmmsv_coll_t *coll, xmms_error_t *err);
static void xmms_playlist_client_add_collection (xmms_playlist_t *playlist, const gchar *plname, xmmsv_coll_t *coll, xmmsv_t *order, xmms_error_t *err);
static GTree * xmms_playlist_client_current_pos (xmms_playlist_t *playlist, const gchar *plname, xmms_error_t *err);
static gint xmms_playlist_client_set_next (xmms_playlist_t *playlist, gint32 pos, xmms_error_t *error);
static void xmms_playlist_client_remove_entry (xmms_playlist_t *playlist, const gchar *plname, gint32 pos, xmms_error_t *err);
static gboolean xmms_playlist_remove_unlocked (xmms_playlist_t *playlist, const gchar *plname, xmmsv_coll_t *plcoll, guint pos, xmms_error_t *err);
static void xmms_playlist_client_move_entry (xmms_playlist_t *playlist, const gchar *plname, gint32 pos, gint32 newpos, xmms_error_t *err);
static gint xmms_playlist_client_set_next_rel (xmms_playlist_t *playlist, gint32 pos, xmms_error_t *error);
static gint xmms_playlist_set_current_position_do (xmms_playlist_t *playlist, guint32 pos, xmms_error_t *err);

static void xmms_playlist_client_insert_url (xmms_playlist_t *playlist, const gchar *plname, gint32 pos, const gchar *url, xmms_error_t *error);
static void xmms_playlist_client_insert_id (xmms_playlist_t *playlist, const gchar *plname, gint32 pos, xmms_medialib_entry_t file, xmms_error_t *error);
static void xmms_playlist_client_insert_collection (xmms_playlist_t *playlist, const gchar *plname, gint32 pos, xmmsv_coll_t *coll, xmmsv_t *order, xmms_error_t *error);
static void xmms_playlist_client_radd (xmms_playlist_t *playlist, const gchar *plname, const gchar *path, xmms_error_t *error);
static void xmms_playlist_client_rinsert (xmms_playlist_t *playlist, const gchar *plname, gint32 pos, const gchar *path, xmms_error_t *error);

static void xmms_playlist_client_load (xmms_playlist_t *, const gchar *, xmms_error_t *);

static xmmsv_coll_t *xmms_playlist_get_coll (xmms_playlist_t *playlist, const gchar *plname, xmms_error_t *error);
static gchar *xmms_playlist_canonical_name (xmms_playlist_t *playlist, const gchar *plname);
static gint xmms_playlist_coll_get_currpos (xmmsv_coll_t *plcoll);
static gint xmms_playlist_coll_get_size (xmmsv_coll_t *plcoll);

static void xmms_playlist_update_queue (xmms_playlist_t *playlist, const gchar *plname, xmmsv_coll_t *coll);
static void xmms_playlist_update_partyshuffle (xmms_playlist_t *playlist, const gchar *plname, xmmsv_coll_t *coll);
static void xmms_playlist_register_ipc_commands (xmms_object_t *playlist_object);

static void xmms_playlist_current_pos_msg_send (xmms_playlist_t *playlist, GTree *dict);
static GTree *xmms_playlist_current_pos_msg_new (xmms_playlist_t *playlist, guint32 pos, const gchar *plname);

static void xmms_playlist_changed_msg_send (xmms_playlist_t *playlist, GTree *dict);
static GTree *xmms_playlist_changed_msg_new (xmms_playlist_t *playlist, xmms_playlist_changed_actions_t type, xmms_medialib_entry_t id, const gchar *plname);

#define XMMS_PLAYLIST_CHANGED_MSG(type, id, name) xmms_playlist_changed_msg_send (playlist, xmms_playlist_changed_msg_new (playlist, type, id, name))
#define XMMS_PLAYLIST_CURRPOS_MSG(pos, name) xmms_playlist_current_pos_msg_send (playlist, xmms_playlist_current_pos_msg_new (playlist, pos, name))


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

	gboolean update_flag;
	xmms_medialib_t *medialib;
};

#include "playlist_ipc.c"

static void
on_playlist_r_all_changed (xmms_object_t *object, xmmsv_t *_data,
                           gpointer udata)
{
	xmms_playlist_t *playlist = udata;
	gint value;

	value = xmms_config_property_get_int ((xmms_config_property_t *) object);

	g_mutex_lock (playlist->mutex);
	playlist->repeat_all = !!value;
	g_mutex_unlock (playlist->mutex);
}

static void
on_playlist_r_one_changed (xmms_object_t *object, xmmsv_t *_data,
                           gpointer udata)
{
	xmms_playlist_t *playlist = udata;
	gint value;

	value = xmms_config_property_get_int ((xmms_config_property_t *) object);

	g_mutex_lock (playlist->mutex);
	playlist->repeat_one = !!value;
	g_mutex_unlock (playlist->mutex);
}


static void
on_playlist_updated (xmms_object_t *object, const gchar *plname)
{
	xmms_playlist_t *playlist = (xmms_playlist_t*)object;
	xmmsv_coll_t *plcoll;
	gchar *type;

	/* Already in an update process, quit */
	if (playlist->update_flag) {
		return;
	}

	plcoll = xmms_playlist_get_coll (playlist, plname, NULL);
	if (plcoll == NULL) {
		return;
	}

	if (!xmmsv_coll_attribute_get (plcoll, "type", &type)) {
		return;
	}

	/* Run the update function if appropriate */
	/* TODO: Service client should be notified here */
	if (strcmp (type, "queue") == 0) {
		xmms_playlist_update_queue (playlist, plname, plcoll);
	} else if (strcmp (type, "pshuffle") == 0) {
		xmms_playlist_update_partyshuffle (playlist, plname, plcoll);
	}
}

static void
on_playlist_updated_pos (xmms_object_t *object, xmmsv_t *val, gpointer udata)
{
	XMMS_DBG ("PLAYLIST: updated pos!");
	on_playlist_updated (object, XMMS_ACTIVE_PLAYLIST);
}

static void
on_playlist_updated_chg (xmms_object_t *object, xmmsv_t *val, gpointer udata)
{
	const gchar *plname = NULL;
	xmmsv_t *pl_val;

	XMMS_DBG ("PLAYLIST: updated chg!");

	xmmsv_dict_get (val, "name", &pl_val);
	if (pl_val != NULL) {
		xmmsv_get_string (pl_val, &plname);
	} else {
		/* FIXME: occurs? */
		XMMS_DBG ("PLAYLIST: updated_chg, NULL playlist!");
		g_assert_not_reached ();
	}

	on_playlist_updated (object, plname);
}

static void
xmms_playlist_update_queue (xmms_playlist_t *playlist, const gchar *plname,
                            xmmsv_coll_t *coll)
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
                                   const gchar *plname, xmmsv_coll_t *coll)
{
	gint history, upcoming, currpos, size;
	xmmsv_coll_t *src;
	xmmsv_t *tmp;

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

	if (!xmmsv_list_get (xmmsv_coll_operands_get (coll), 0, &tmp)) {
		XMMS_DBG ("Cannot find party shuffle operand!");
		return;
	}
	xmmsv_get_coll (tmp, &src);

	currpos = xmms_playlist_coll_get_currpos (coll);
	size = xmms_playlist_coll_get_size (coll);
	while (size < currpos + 1 + upcoming) {
		xmms_medialib_entry_t randentry;
		randentry = xmms_collection_get_random_media (playlist->colldag, src);
		if (randentry == 0) {
			break;  /* No media found in the collection, give up */
		}
		/* FIXME: add_collection might yield better perf here. */
		xmms_playlist_add_entry_unlocked (playlist, plname, coll, randentry, NULL);

		currpos = xmms_playlist_coll_get_currpos (coll);
		size = xmms_playlist_coll_get_size (coll);
	}
	playlist->update_flag = FALSE;
}

typedef struct {
	xmms_playlist_t *pls;
	xmms_medialib_entry_t entry;
} playlist_remove_context_t;

static void
remove_from_playlist (gpointer key, gpointer value, gpointer udata)
{
	playlist_remove_context_t *ctx = (playlist_remove_context_t *) udata;
	xmmsv_coll_t *coll = (xmmsv_coll_t *) value;
	const gchar *name = (const gchar *) key;

	xmms_medialib_entry_t val;
	guint32 i;

	for (i = 0; xmmsv_coll_idlist_get_index (coll, i, &val); i++) {
		if (val == ctx->entry) {
			XMMS_DBG ("removing entry on pos %d in %s", i, name);
			xmms_playlist_remove_unlocked (ctx->pls, name, coll, i, NULL);
			i--; /* reset it */
		}
	}
}

static void
on_medialib_entry_removed (xmms_object_t *object, xmmsv_t *val, gpointer udata)
{
	xmms_playlist_t *playlist = (xmms_playlist_t *) udata;
	playlist_remove_context_t ctx;
	gint entry;

	g_return_if_fail (playlist);
	g_return_if_fail (xmmsv_get_int (val, &entry));

	ctx.pls = playlist;
	ctx.entry = entry;

	g_mutex_lock (playlist->mutex);

	xmms_collection_foreach_in_namespace (playlist->colldag,
	                                      XMMS_COLLECTION_NSID_PLAYLISTS,
	                                      remove_from_playlist, &ctx);

	g_mutex_unlock (playlist->mutex);
}

static void
on_collection_changed (xmms_object_t *object, xmmsv_t *val, gpointer udata)
{
	xmms_playlist_t *playlist = (xmms_playlist_t *) udata;
	const gchar *ns, *name;

	g_return_if_fail (playlist);
	g_return_if_fail (xmmsv_is_type (val, XMMSV_TYPE_DICT));
	g_return_if_fail (xmmsv_dict_entry_get_string (val, "namespace", &ns));

	if (strcmp (ns, XMMS_COLLECTION_NS_PLAYLISTS) != 0) {
		/* Not a playlist, not our concern... */
		return;
	}

	g_return_if_fail (xmmsv_dict_entry_get_string (val, "name", &name));

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_UPDATE, 0, name);
}

/**
 * Initializes a new xmms_playlist_t.
 */
xmms_playlist_t *
xmms_playlist_init (xmms_medialib_t *medialib, xmms_coll_dag_t *colldag)
{
	xmms_playlist_t *ret;
	xmms_config_property_t *val;

	ret = xmms_object_new (xmms_playlist_t, xmms_playlist_destroy);
	ret->mutex = g_mutex_new ();

	xmms_playlist_register_ipc_commands (XMMS_OBJECT (ret));

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

	xmms_object_ref (medialib);
	ret->medialib = medialib;

	xmms_object_ref (colldag);
	ret->colldag = colldag;

	xmms_object_connect (XMMS_OBJECT (ret->medialib),
	                     XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_REMOVED,
	                     on_medialib_entry_removed, ret);

	xmms_object_connect (XMMS_OBJECT (ret->colldag),
	                     XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                     on_collection_changed, ret);

	return ret;
}

static gboolean
xmms_playlist_advance_do (xmms_playlist_t *playlist)
{
	gint size, currpos;
	gboolean ret = TRUE;
	xmmsv_coll_t *plcoll;
	char *jumplist;
	xmms_error_t err;
	xmms_playlist_t *buffer = playlist;
	guint newpos;

	xmms_error_reset (&err);

	plcoll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, NULL);
	if (plcoll == NULL) {
		ret = FALSE;
	} else if ((size = xmms_playlist_coll_get_size (plcoll)) == 0) {
		if (xmmsv_coll_attribute_get (plcoll, "jumplist", &jumplist)) {
			xmms_playlist_client_load (buffer, jumplist, &err);
			if (xmms_error_isok (&err)) {
				ret = xmms_playlist_advance_do (playlist);
			} else {
				ret = FALSE;
			}
		} else {
			ret = FALSE;
		}
	} else if (!playlist->repeat_one) {
		currpos = xmms_playlist_coll_get_currpos (plcoll);
		currpos++;

		if (currpos == size && !playlist->repeat_all &&
		    xmmsv_coll_attribute_get (plcoll, "jumplist", &jumplist)) {

			xmms_collection_set_int_attr (plcoll, "position", -1);
			XMMS_PLAYLIST_CURRPOS_MSG (-1, XMMS_ACTIVE_PLAYLIST);

			xmms_playlist_client_load (buffer, jumplist, &err);
			if (xmms_error_isok (&err)) {
				ret = xmms_playlist_advance_do (playlist);
			} else {
				ret = FALSE;
			}
		} else {
			newpos = currpos%size;
			xmms_collection_set_int_attr (plcoll, "position", newpos);
			XMMS_PLAYLIST_CURRPOS_MSG (newpos, XMMS_ACTIVE_PLAYLIST);
			ret = (currpos != size) || playlist->repeat_all;
		}
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
	gboolean ret;

	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);
	ret = xmms_playlist_advance_do (playlist);
	g_mutex_unlock (playlist->mutex);

	return ret;
}

/**
 * Retrieve the currently active xmms_medialib_entry_t.
 *
 */
xmms_medialib_entry_t
xmms_playlist_current_entry (xmms_playlist_t *playlist)
{
	gint size, currpos;
	xmmsv_coll_t *plcoll;
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
		xmmsv_coll_idlist_get_index (plcoll, currpos, &ent);
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
GTree *
xmms_playlist_client_current_pos (xmms_playlist_t *playlist, const gchar *plname,
                                  xmms_error_t *err)
{
	guint32 pos;
	xmmsv_coll_t *plcoll;
	GTree *dict;

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

	dict = xmms_playlist_current_pos_msg_new (playlist, pos, plname);

	return dict;
}

/**
 * Retrieve a copy of the name of the currently active playlist.
 *
 */
static gchar *
xmms_playlist_client_current_active (xmms_playlist_t *playlist, xmms_error_t *err)
{
	gchar *name = NULL;
	xmmsv_coll_t *active_coll;

	g_return_val_if_fail (playlist, 0);

	g_mutex_lock (playlist->mutex);

	active_coll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, err);
	if (active_coll != NULL) {
		const gchar *alias;

		alias = xmms_collection_find_alias (playlist->colldag,
		                                    XMMS_COLLECTION_NSID_PLAYLISTS,
		                                    active_coll, XMMS_ACTIVE_PLAYLIST);
		if (alias == NULL) {
			xmms_error_set (err, XMMS_ERROR_GENERIC, "active playlist not referenced!");
		} else {
			name = g_strdup (alias);
		}
	} else {
		xmms_error_set (err, XMMS_ERROR_GENERIC, "no active playlist");
	}

	g_mutex_unlock (playlist->mutex);

	return name;
}


static void
xmms_playlist_client_load (xmms_playlist_t *playlist, const gchar *name, xmms_error_t *err)
{
	xmmsv_coll_t *plcoll, *active_coll;

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
	                    XMMSV_TYPE_STRING,
	                    name);
}

static inline void
swap_entries (xmmsv_coll_t *coll, gint i, gint j)
{
	xmms_medialib_entry_t tmp, tmp2;

	xmmsv_coll_idlist_get_index (coll, i, &tmp);
	xmmsv_coll_idlist_get_index (coll, j, &tmp2);

	xmmsv_coll_idlist_set_index (coll, i, tmp2);
	xmmsv_coll_idlist_set_index (coll, j, tmp);
}


/**
 * Shuffle the playlist.
 *
 */
static void
xmms_playlist_client_shuffle (xmms_playlist_t *playlist, const gchar *plname,
                              xmms_error_t *err)
{
	guint j,i;
	gint len, currpos;
	xmmsv_coll_t *plcoll;

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
                               xmmsv_coll_t *plcoll, guint pos, xmms_error_t *err)
{
	gint currpos;
	GTree *dict;

	g_return_val_if_fail (playlist, FALSE);

	currpos = xmms_playlist_coll_get_currpos (plcoll);

	if (!xmmsv_coll_idlist_remove (plcoll, pos)) {
		if (err) xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		return FALSE;
	}

	dict = xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_REMOVE, 0, plname);
	g_tree_insert (dict, (gpointer) "position", xmmsv_new_int (pos));
	xmms_playlist_changed_msg_send (playlist, dict);

	/* decrease currentpos if removed entry was before or if it's
	 * the current entry, but only if currentpos is a valid entry.
	 */
	if (currpos != -1 && pos <= currpos) {
		currpos--;
		xmms_collection_set_int_attr (plcoll, "position", currpos);
		XMMS_PLAYLIST_CURRPOS_MSG (currpos, plname);
	}

	return TRUE;
}

/**
 * Remove an entry from playlist.
 *
 */
void
xmms_playlist_client_remove_entry (xmms_playlist_t *playlist,
                                   const gchar *plname,
                                   gint32 pos, xmms_error_t *err)
{
	gboolean ret = FALSE;
	xmmsv_coll_t *plcoll;

	g_return_if_fail (playlist);

	g_mutex_lock (playlist->mutex);
	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll != NULL) {
		ret = xmms_playlist_remove_unlocked (playlist, plname, plcoll, pos, err);
	}
	g_mutex_unlock (playlist->mutex);
}


/**
 * Move an entry in playlist
 *
 */
static void
xmms_playlist_client_move_entry (xmms_playlist_t *playlist,
                                 const gchar *plname, gint32 pos,
                                 gint32 newpos, xmms_error_t *err)
{
	GTree *dict;
	xmms_medialib_entry_t id;
	gint currpos, size;
	gint64 ipos, inewpos;
	xmmsv_coll_t *plcoll;

	g_return_if_fail (playlist);

	XMMS_DBG ("Moving %d, to %d", pos, newpos);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		/* FIXME: happens ? */
		g_mutex_unlock (playlist->mutex);
		return;
	}

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	size = xmms_playlist_coll_get_size (plcoll);

	if (size == 0 || newpos > (size - 1)) {
		xmms_error_set (err, XMMS_ERROR_NOENT,
		                "Cannot move entry outside playlist");
		g_mutex_unlock (playlist->mutex);
		return;
	}

	if (!xmmsv_coll_idlist_move (plcoll, pos, newpos)) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Entry was not in list!");
		g_mutex_unlock (playlist->mutex);
		return;
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

	xmmsv_coll_idlist_get_index (plcoll, newpos, &id);

	dict = xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_MOVE, id, plname);
	g_tree_insert (dict, (gpointer) "position", xmmsv_new_int (pos));
	g_tree_insert (dict, (gpointer) "newposition", xmmsv_new_int (newpos));
	xmms_playlist_changed_msg_send (playlist, dict);

	XMMS_PLAYLIST_CURRPOS_MSG (currpos, plname);

	g_mutex_unlock (playlist->mutex);

	return;

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
static void
xmms_playlist_client_insert_url (xmms_playlist_t *playlist, const gchar *plname,
                                 gint32 pos, const gchar *url, xmms_error_t *err)
{
	xmms_medialib_entry_t entry = 0;

	MEDIALIB_SESSION (playlist->medialib,
	                  entry = xmms_medialib_entry_new_encoded (session, url, err));

	if (!entry) {
		return;
	}

	xmms_playlist_client_insert_id (playlist, plname, pos, entry, err);
}

/**
  * Convenient function for inserting a directory at a given position
  * in the playlist, It will dive down the URL you feed it and
  * recursivly insert all files.
  *
  * @param playlist the playlist to add it URL to.
  * @param plname the name of the playlist to modify.
  * @param pos a position in the playlist.
  * @param nurl the URL of an directory you want to add
  * @param err an #xmms_error_t that should be defined upon error.
  */
static void
xmms_playlist_client_rinsert (xmms_playlist_t *playlist, const gchar *plname, gint32 pos,
                              const gchar *path, xmms_error_t *err)
{
	xmmsv_coll_t *idlist;
	xmmsv_list_iter_t *it;
	xmmsv_t *list;

	idlist = xmms_medialib_add_recursive (playlist->medialib, path, err);
	list = xmmsv_coll_idlist_get (idlist);

	xmmsv_get_list_iter (list, &it);
	xmmsv_list_iter_last (it);
	while (xmmsv_list_iter_valid (it)) {
		xmms_medialib_entry_t entry;
		xmmsv_t *value;

		xmmsv_list_iter_entry (it, &value);
		xmmsv_get_int (value, &entry);

		xmms_playlist_insert_entry (playlist, plname, pos, entry, err);

		xmmsv_list_iter_prev (it);
	}

	xmmsv_coll_unref (idlist);
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
static void
xmms_playlist_client_insert_id (xmms_playlist_t *playlist, const gchar *plname,
                                gint32 pos, xmms_medialib_entry_t file,
                                xmms_error_t *err)
{
	xmms_playlist_insert_entry (playlist, plname, pos, file, err);
}

static void
xmms_playlist_client_insert_collection (xmms_playlist_t *playlist, const gchar *plname,
                                        gint32 pos, xmmsv_coll_t *coll,
                                        xmmsv_t *order, xmms_error_t *err)
{
	xmmsv_t *res;
	int32_t id;
	int i;

	res = xmms_collection_query_ids (playlist->colldag, coll, 0, 0, order, err);

	for (i = 0; xmmsv_list_get_int (res, i, &id); i++) {
		xmms_playlist_client_insert_id (playlist, plname, pos, id, err);
	}

}

/**
 * Insert an entry at a given position in the playlist without
 * validating it.
 *
 * @internal
 */
void
xmms_playlist_insert_entry (xmms_playlist_t *playlist, const gchar *plname,
                            guint32 pos, xmms_medialib_entry_t file,
                            xmms_error_t *err)
{
	GTree *dict;
	gint currpos;
	gint len;
	xmmsv_coll_t *plcoll;
	gboolean valid;

	g_mutex_lock (playlist->mutex);

	MEDIALIB_SESSION (playlist->medialib, valid = xmms_medialib_check_id (session, file));
	if (!valid) {
		g_mutex_unlock (playlist->mutex);
		xmms_error_set (err, XMMS_ERROR_NOENT,
		                "That is not a valid medialib id!");
		return;
	}


	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		/* FIXME: happens ? */
		g_mutex_unlock (playlist->mutex);
		return;
	}

	len = xmms_playlist_coll_get_size (plcoll);
	if (pos > len) {
		xmms_error_set (err, XMMS_ERROR_GENERIC,
		                "Could not insert entry outside of playlist!");
		g_mutex_unlock (playlist->mutex);
		return;
	}
	xmmsv_coll_idlist_insert (plcoll, pos, file);

	/** propagate the MID ! */
	dict = xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_INSERT, file, plname);
	g_tree_insert (dict, (gpointer) "position", xmmsv_new_int (pos));
	xmms_playlist_changed_msg_send (playlist, dict);

	/** update position once client is familiar with the new item. */
	currpos = xmms_playlist_coll_get_currpos (plcoll);
	if (pos <= currpos) {
		currpos++;
		xmms_collection_set_int_attr (plcoll, "position", currpos);
		XMMS_PLAYLIST_CURRPOS_MSG (currpos, plname);
	}

	g_mutex_unlock (playlist->mutex);
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
void
xmms_playlist_client_add_url (xmms_playlist_t *playlist, const gchar *plname,
                              const gchar *nurl, xmms_error_t *err)
{
	xmms_medialib_entry_t entry = 0;

	MEDIALIB_SESSION (playlist->medialib,
	                  entry = xmms_medialib_entry_new_encoded (session, nurl, err));

	if (entry) {
		xmms_playlist_add_entry (playlist, plname, entry, err);
	}

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
xmms_playlist_client_radd (xmms_playlist_t *playlist, const gchar *plname,
                           const gchar *path, xmms_error_t *err)
{
	xmmsv_coll_t *idlist;
	xmmsv_list_iter_t *it;
	xmmsv_t *list;

	idlist = xmms_medialib_add_recursive (playlist->medialib, path, err);
	list = xmmsv_coll_idlist_get (idlist);

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_valid (it)) {
		xmms_medialib_entry_t entry;
		xmmsv_t *value;

		xmmsv_list_iter_entry (it, &value);
		xmmsv_get_int (value, &entry);

		xmms_playlist_add_entry (playlist, plname, entry, err);

		xmmsv_list_iter_next (it);
	}
	xmmsv_coll_unref (idlist);
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

void
xmms_playlist_client_add_id (xmms_playlist_t *playlist, const gchar *plname,
                             xmms_medialib_entry_t file, xmms_error_t *err)
{
	gboolean valid;

	MEDIALIB_SESSION (playlist->medialib, valid = xmms_medialib_check_id (session, file));

	if (!valid) {
		xmms_error_set (err, XMMS_ERROR_NOENT,
		                "That is not a valid medialib id!");
		return;
	}

	xmms_playlist_add_entry (playlist, plname, file, err);
}

void
xmms_playlist_client_add_idlist (xmms_playlist_t *playlist,
                                 const gchar *plname,
                                 xmmsv_coll_t *coll, xmms_error_t *err)
{
	xmms_medialib_entry_t entry;
	xmmsv_list_iter_t *it;
	gboolean valid;


	xmmsv_get_list_iter (xmmsv_coll_idlist_get (coll), &it);
	for (xmmsv_list_iter_first (it);
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it)) {

		xmmsv_list_iter_entry_int (it, &entry);
		MEDIALIB_SESSION (playlist->medialib,
		                  valid = xmms_medialib_check_id (session, entry));

		if (!valid) {
			xmms_error_set (err, XMMS_ERROR_NOENT,
			                "Idlist contains invalid medialib id!");
			xmmsv_list_iter_explicit_destroy (it);
			return;
		}
	}

	for (xmmsv_list_iter_first (it);
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it)) {

		xmmsv_list_iter_entry_int (it, &entry);
		xmms_playlist_add_entry (playlist, plname, entry, err);
	}
	xmmsv_list_iter_explicit_destroy (it);

}

void
xmms_playlist_client_add_collection (xmms_playlist_t *playlist, const gchar *plname,
                                     xmmsv_coll_t *coll, xmmsv_t *order,
                                     xmms_error_t *err)
{
	xmmsv_t *res;
	int32_t id;
	int i;

	res = xmms_collection_query_ids (playlist->colldag, coll, 0, 0, order, err);

	for (i = 0; xmmsv_list_get_int (res, i, &id); i++) {
		xmms_playlist_add_entry (playlist, plname, id, err);
	}

	xmmsv_unref (res);
}

/**
 * Add an entry to the playlist without validating it.
 *
 * @internal
 */
void
xmms_playlist_add_entry (xmms_playlist_t *playlist, const gchar *plname,
                         xmms_medialib_entry_t file, xmms_error_t *err)
{
	xmmsv_coll_t *plcoll;

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
                                  xmmsv_coll_t *plcoll,
                                  xmms_medialib_entry_t file,
                                  xmms_error_t *err)
{
	gint prev_size;
	GTree *dict;

	prev_size = xmms_playlist_coll_get_size (plcoll);
	xmmsv_coll_idlist_append (plcoll, file);

	/** propagate the MID ! */
	dict = xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_ADD, file, plname);
	g_tree_insert (dict, (gpointer) "position", xmmsv_new_int (prev_size));
	xmms_playlist_changed_msg_send (playlist, dict);
}

/** Clear the playlist */
static void
xmms_playlist_client_clear (xmms_playlist_t *playlist, const gchar *plname,
                            xmms_error_t *err)
{
	xmmsv_coll_t *plcoll;

	g_return_if_fail (playlist);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		g_mutex_unlock (playlist->mutex);
		return;
	}

	xmmsv_coll_idlist_clear (plcoll);
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

static gint
xmms_playlist_set_current_position_do (xmms_playlist_t *playlist, guint32 pos,
                                       xmms_error_t *err)
{
	gint size;
	xmms_medialib_entry_t mid;
	xmmsv_coll_t *plcoll;
	char *jumplist;

	g_return_val_if_fail (playlist, FALSE);

	plcoll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, err);
	if (plcoll == NULL) {
		return 0;
	}

	size = xmms_playlist_coll_get_size (plcoll);

	if (pos == size &&
	    xmmsv_coll_attribute_get (plcoll, "jumplist", &jumplist)) {

		xmms_collection_set_int_attr (plcoll, "position", 0);
		XMMS_PLAYLIST_CURRPOS_MSG (0, XMMS_ACTIVE_PLAYLIST);

		xmms_playlist_client_load (playlist, jumplist, err);
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

	xmmsv_coll_idlist_get_index (plcoll, pos, &mid);

	return mid;
}

gint
xmms_playlist_client_set_next (xmms_playlist_t *playlist, gint32 pos,
                               xmms_error_t *err)
{
	xmms_medialib_entry_t mid;
	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);
	mid = xmms_playlist_set_current_position_do (playlist, pos, err);
	g_mutex_unlock (playlist->mutex);

	return mid;
}

static gint
xmms_playlist_client_set_next_rel (xmms_playlist_t *playlist, gint32 pos,
                                   xmms_error_t *err)
{
	gint currpos, newpos, size;
	xmms_medialib_entry_t mid = 0;
	xmmsv_coll_t *plcoll;

	g_return_val_if_fail (playlist, FALSE);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, XMMS_ACTIVE_PLAYLIST, err);
	if (plcoll != NULL) {
		currpos = xmms_playlist_coll_get_currpos (plcoll);

		if (playlist->repeat_all) {
			newpos = pos + currpos;
			size = (gint) xmmsv_coll_idlist_get_size (plcoll);

			if (size > 0) {
				newpos %= size;
				if (newpos < 0) {
					newpos += size;
				}
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
xmms_playlist_client_sort (xmms_playlist_t *playlist, const gchar *plname,
                           xmmsv_t *properties, xmms_error_t *err)
{

	xmmsv_t *tmp, *idlist, *val;
	xmmsv_coll_t *plcoll;
	gint currpos, pos;
	xmms_medialib_entry_t currid;

	g_return_if_fail (playlist);
	g_return_if_fail (properties);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "no such playlist!");
		g_mutex_unlock (playlist->mutex);
		return;
	}

	currpos = xmms_playlist_coll_get_currpos (plcoll);
	xmmsv_coll_idlist_get_index (plcoll, currpos, &currid);

	/* Let the collection code do the sorting for us */
	tmp = xmms_collection_query_ids (playlist->colldag, plcoll, 0, 0,
	                                 properties, err);

	if (tmp == NULL) {
		g_mutex_unlock (playlist->mutex);
		return;
	}

	idlist = xmmsv_coll_idlist_get (plcoll);
	xmmsv_list_clear (idlist);

	for (pos = 0; xmmsv_list_get (tmp, pos, &val); pos++) {
		xmms_medialib_entry_t id;

		xmmsv_get_int (val, &id);
		xmmsv_list_append (idlist, val);

		if (id == currid) {
			xmms_collection_set_int_attr (plcoll, "position", pos);
			currpos = pos;
		}
	}

	xmmsv_unref (tmp);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SORT, 0, plname);
	XMMS_PLAYLIST_CURRPOS_MSG (currpos, plname);

	g_mutex_unlock (playlist->mutex);
}


/** List a playlist */
static GList *
xmms_playlist_client_list_entries (xmms_playlist_t *playlist, const gchar *plname,
                                   xmms_error_t *err)
{
	GList *entries = NULL;
	xmmsv_coll_t *plcoll;
	xmms_medialib_entry_t entry;
	xmmsv_list_iter_t *it;

	g_return_val_if_fail (playlist, NULL);

	g_mutex_lock (playlist->mutex);

	plcoll = xmms_playlist_get_coll (playlist, plname, err);
	if (plcoll == NULL) {
		g_mutex_unlock (playlist->mutex);
		return NULL;
	}

	xmmsv_get_list_iter (xmmsv_coll_idlist_get (plcoll), &it);
	for (xmmsv_list_iter_first (it);
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it)) {

		xmmsv_list_iter_entry_int (it, &entry);
		entries = g_list_prepend (entries, xmmsv_new_int (entry));
	}
	xmmsv_list_iter_explicit_destroy (it);

	g_mutex_unlock (playlist->mutex);

	entries = g_list_reverse (entries);

	return entries;
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
	xmms_playlist_t *playlist = (xmms_playlist_t *) object;

	XMMS_DBG ("Deactivating playlist object");

	g_return_if_fail (playlist);

	val = xmms_config_lookup ("playlist.repeat_one");
	xmms_config_property_callback_remove (val, on_playlist_r_one_changed, playlist);

	val = xmms_config_lookup ("playlist.repeat_all");
	xmms_config_property_callback_remove (val, on_playlist_r_all_changed, playlist);

	xmms_object_disconnect (XMMS_OBJECT (playlist),
	                        XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	                        on_playlist_updated_chg, playlist);

	xmms_object_disconnect (XMMS_OBJECT (playlist),
	                        XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                        on_playlist_updated_pos, playlist);

	xmms_object_disconnect (XMMS_OBJECT (playlist->medialib),
	                        XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_REMOVED,
	                        on_medialib_entry_removed, playlist);

	xmms_object_disconnect (XMMS_OBJECT (playlist->colldag),
	                        XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                        on_collection_changed, playlist);

	xmms_object_unref (playlist->colldag);
	xmms_object_unref (playlist->medialib);

	g_mutex_free (playlist->mutex);

	xmms_playlist_unregister_ipc_commands ();
}


static xmmsv_coll_t *
xmms_playlist_get_coll (xmms_playlist_t *playlist, const gchar *plname,
                        xmms_error_t *error)
{
	xmmsv_coll_t *coll;
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
static gchar *
xmms_playlist_canonical_name (xmms_playlist_t *playlist, const gchar *plname)
{
	gchar *fullname;

	if (strcmp (plname, XMMS_ACTIVE_PLAYLIST) == 0) {
		xmmsv_coll_t *coll;
		coll = xmms_collection_get_pointer (playlist->colldag, plname,
		                                    XMMS_COLLECTION_NSID_PLAYLISTS);
		fullname = xmms_collection_find_alias (playlist->colldag,
		                                       XMMS_COLLECTION_NSID_PLAYLISTS,
		                                       coll, plname);
	} else {
		fullname = g_strdup (plname);
	}

	return fullname;
}

/** Get the current position in the given playlist (set to -1 if absent). */
static gint
xmms_playlist_coll_get_currpos (xmmsv_coll_t *plcoll)
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
xmms_playlist_coll_get_size (xmmsv_coll_t *plcoll)
{
	return xmmsv_coll_idlist_get_size (plcoll);
}


GTree *
xmms_playlist_changed_msg_new (xmms_playlist_t *playlist,
                               xmms_playlist_changed_actions_t type,
                               xmms_medialib_entry_t id, const gchar *plname)
{
	GTree *dict;
	gchar *tmp;

	dict = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
	                        NULL, (GDestroyNotify) xmmsv_unref);

	g_tree_insert (dict, (gpointer) "type", xmmsv_new_int (type));

	if (id) {
		g_tree_insert (dict, (gpointer) "id", xmmsv_new_int (id));
	}

	tmp = xmms_playlist_canonical_name (playlist, plname);
	g_tree_insert (dict, (gpointer) "name", xmmsv_new_string (tmp));
	g_free (tmp);

	return dict;
}

GTree *
xmms_playlist_current_pos_msg_new (xmms_playlist_t *playlist,
                                   guint32 pos, const gchar *plname)
{
	GTree *dict;
	gchar *tmp;

	dict = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
	                        NULL, (GDestroyNotify) xmmsv_unref);

	g_tree_insert (dict, (gpointer) "position", xmmsv_new_int (pos));

	tmp = xmms_playlist_canonical_name (playlist, plname);
	g_tree_insert (dict, (gpointer) "name", xmmsv_new_string (tmp));
	g_free (tmp);

	return dict;
}

void
xmms_playlist_changed_msg_send (xmms_playlist_t *playlist, GTree *dict)
{
	xmmsv_t *type_val;
	xmmsv_t *pl_val;
	gint type;
	const gchar *plname;

	g_return_if_fail (playlist);
	g_return_if_fail (dict);

	/* If local playlist change, trigger a COLL_CHANGED signal */
	type_val = g_tree_lookup (dict, "type");
	pl_val = g_tree_lookup (dict, "name");
	if (type_val != NULL && xmmsv_get_int (type_val, &type) &&
	    type != XMMS_PLAYLIST_CHANGED_UPDATE &&
	    pl_val != NULL && xmmsv_get_string (pl_val, &plname)) {
		XMMS_COLLECTION_PLAYLIST_CHANGED_MSG (playlist->colldag, plname);
	}

	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	                    XMMSV_TYPE_DICT,
	                    dict);

	g_tree_destroy (dict);
}

static void
xmms_playlist_current_pos_msg_send (xmms_playlist_t *playlist,
                                   GTree *dict)
{
	g_return_if_fail (playlist);

	g_return_if_fail (dict);

	xmms_object_emit_f (XMMS_OBJECT (playlist),
	                    XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                    XMMSV_TYPE_DICT,
	                    dict);

	g_tree_destroy (dict);
}
