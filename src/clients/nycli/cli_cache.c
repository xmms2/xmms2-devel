/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#include <string.h>

#include "main.h"
#include "cli_cache.h"
#include "xmmscall.h"

static void
freshness_init (freshness_t *fresh)
{
	fresh->status = CLI_CACHE_NOT_INIT;
	fresh->pending_queries = 0;
}

static gboolean
freshness_is_fresh (freshness_t *fresh)
{
	return fresh->status == CLI_CACHE_FRESH;
}

static void
freshness_requested (freshness_t *fresh)
{
	fresh->status = CLI_CACHE_PENDING;
	fresh->pending_queries++;
}

static void
freshness_received (freshness_t *fresh)
{
	if (fresh->status == CLI_CACHE_NOT_INIT) {
		fresh->status = CLI_CACHE_FRESH;
		fresh->pending_queries = 0;
	} else if (fresh->status == CLI_CACHE_PENDING) {
		if (fresh->pending_queries > 0) {
			fresh->pending_queries--;
		}
		if (fresh->pending_queries == 0) {
			fresh->status = CLI_CACHE_FRESH;
		}
	}
}

static gint
refresh_currpos (xmmsv_t *val, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;

	if (!xmmsv_is_error (val)) {
		xmmsv_dict_entry_get_int (val, "position", &cache->currpos);
	} else {
		/* Current pos not set */
		cache->currpos = -1;
	}

	freshness_received (&cache->freshness_currpos);

	return TRUE;
}

static gint
refresh_currid (xmmsv_t *val, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;

	if (!xmmsv_is_error (val)) {
		xmmsv_get_int (val, &cache->currid);
	}

	freshness_received (&cache->freshness_currid);

	return TRUE;
}

static gint
refresh_playback_status (xmmsv_t *val, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;

	if (!xmmsv_is_error (val)) {
		xmmsv_get_int (val, &cache->playback_status);
	}

	freshness_received (&cache->freshness_playback_status);

	return TRUE;
}

static gint
refresh_active_playlist_name (xmmsv_t *val, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;
	const gchar *buf;

	if (!xmmsv_is_error (val) && xmmsv_get_string (val, &buf)) {
		g_free (cache->active_playlist_name);
		cache->active_playlist_name = g_strdup (buf);
	}

	freshness_received (&cache->freshness_active_playlist_name);

	return TRUE;
}

static gint
refresh_active_playlist (xmmsv_t *val, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;

	if (!xmmsv_is_error (val)) {
		xmmsv_unref (cache->active_playlist);
		cache->active_playlist = xmmsv_ref (val);
	}

	freshness_received (&cache->freshness_active_playlist);

	return TRUE;
}

static gint
update_active_playlist (xmmsv_t *val, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;
	xmmsc_result_t *refres;
	gint pos, newpos, type;
	gint id;
	const gchar *name;

	xmmsv_dict_entry_get_int (val, "type", &type);
	xmmsv_dict_entry_get_int (val, "position", &pos);
	xmmsv_dict_entry_get_int (val, "id", &id);
	xmmsv_dict_entry_get_string (val, "name", &name);

	/* Active playlist not changed, nevermind */
	if (strcmp (name, cache->active_playlist_name) != 0) {
		return TRUE;
	}

	/* Apply changes to the cached playlist */
	switch (type) {
	case XMMS_PLAYLIST_CHANGED_ADD:
		xmmsv_list_append_int (cache->active_playlist, id);
		break;

	case XMMS_PLAYLIST_CHANGED_INSERT:
		xmmsv_list_insert_int (cache->active_playlist, pos, id);
		break;

	case XMMS_PLAYLIST_CHANGED_MOVE:
		xmmsv_dict_entry_get_int (val, "newposition", &newpos);
		xmmsv_list_remove (cache->active_playlist, pos);
		xmmsv_list_insert_int (cache->active_playlist, newpos, id);
		break;

	case XMMS_PLAYLIST_CHANGED_REMOVE:
		xmmsv_list_remove (cache->active_playlist, pos);
		break;

	case XMMS_PLAYLIST_CHANGED_SHUFFLE:
	case XMMS_PLAYLIST_CHANGED_SORT:
	case XMMS_PLAYLIST_CHANGED_CLEAR:
		/* Oops, reload the whole playlist */
		refres = xmmsc_playlist_list_entries (cache->conn, XMMS_ACTIVE_PLAYLIST);
		xmmsc_result_notifier_set (refres, &refresh_active_playlist, cache);
		xmmsc_result_unref (refres);
		freshness_requested (&cache->freshness_active_playlist);
		break;
	}

	return TRUE;
}

static gint
reload_active_playlist (xmmsv_t *val, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;
	xmmsc_result_t *refres;
	const gchar *buf;

	/* FIXME: Also listen to playlist renames, in case the active PL is renamed! */
	/* Refresh playlist name */
	if (xmmsv_get_string (val, &buf)) {
		g_free (cache->active_playlist_name);
		cache->active_playlist_name = g_strdup (buf);
	}

	/* Get all the entries again */
	refres = xmmsc_playlist_list_entries (cache->conn, XMMS_ACTIVE_PLAYLIST);
	xmmsc_result_notifier_set (refres, &refresh_active_playlist, cache);
	xmmsc_result_unref (refres);
	freshness_requested (&cache->freshness_active_playlist);

	return TRUE;
}

static gint
update_active_playlist_name (xmmsv_t *val, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;
	gint type;
	const gchar *name, *newname;

	xmmsv_dict_entry_get_int (val, "type", &type);
	xmmsv_dict_entry_get_string (val, "name", &name);

	/* Active playlist have not been renamed */
	if (strcmp (name, cache->active_playlist_name) != 0) {
		return TRUE;
	}

	if (type == XMMS_COLLECTION_CHANGED_RENAME) {
		g_free (cache->active_playlist_name);
		xmmsv_dict_entry_get_string (val, "newname", &newname);
		cache->active_playlist_name = g_strdup (newname);
	}

	return TRUE;
}

/** Initialize the cache, must still be started to be filled. */
cli_cache_t *
cli_cache_init ()
{
	cli_cache_t *cache;

	cache = g_new0 (cli_cache_t, 1);
	cache->currpos = -1;
	cache->currid = 0;
	cache->playback_status = 0;
	cache->active_playlist = xmmsv_new_list ();
	cache->active_playlist_name = NULL;

	/* Init the freshness state */
	freshness_init (&cache->freshness_currpos);
	freshness_init (&cache->freshness_currid);
	freshness_init (&cache->freshness_playback_status);
	freshness_init (&cache->freshness_active_playlist);
	freshness_init (&cache->freshness_active_playlist_name);

	return cache;
}

void
cli_cache_refresh (cli_cache_t *cache)
{
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_current_pos, cache->conn, XMMS_ACTIVE_PLAYLIST),
	                 FUNC_CALL_P (refresh_currpos, XMMS_PREV_VALUE, cache));

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playback_current_id, cache->conn),
	                 FUNC_CALL_P (refresh_currid, XMMS_PREV_VALUE, cache));

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playback_status, cache->conn),
	                 FUNC_CALL_P (refresh_playback_status, XMMS_PREV_VALUE, cache));

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_list_entries, cache->conn, XMMS_ACTIVE_PLAYLIST),
	                 FUNC_CALL_P (refresh_active_playlist, XMMS_PREV_VALUE, cache));

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_current_active, cache->conn),
	                 FUNC_CALL_P (refresh_active_playlist_name, XMMS_PREV_VALUE, cache));
}

/** Fill the cache with initial (current) data, setup listeners. */
void
cli_cache_start (cli_cache_t *cache, xmmsc_connection_t *conn)
{
	xmmsc_result_t *res;

	g_return_if_fail (cache->conn == NULL);

	cache->conn = xmmsc_ref (conn);

	/* Setup async listeners */
	res = xmmsc_broadcast_playlist_current_pos (conn);
	xmmsc_result_notifier_set (res, &refresh_currpos, cache);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playback_current_id (conn);
	xmmsc_result_notifier_set (res, &refresh_currid, cache);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playback_status (conn);
	xmmsc_result_notifier_set (res, &refresh_playback_status, cache);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playlist_changed (conn);
	xmmsc_result_notifier_set (res, &update_active_playlist, cache);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playlist_loaded (conn);
	xmmsc_result_notifier_set (res, &reload_active_playlist, cache);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_collection_changed (conn);
	xmmsc_result_notifier_set (res, &update_active_playlist_name, cache);
	xmmsc_result_unref (res);

	/* Setup one-time value fetchers, for init */
	cli_cache_refresh (cache);
}

/** Check whether the cache is currently fresh (up-to-date). */
gboolean
cli_cache_is_fresh (cli_cache_t *cache)
{
	/* Check if all items are fresh */
	return freshness_is_fresh (&cache->freshness_currpos) &&
	       freshness_is_fresh (&cache->freshness_currid) &&
	       freshness_is_fresh (&cache->freshness_playback_status) &&
	       freshness_is_fresh (&cache->freshness_active_playlist) &&
	       freshness_is_fresh (&cache->freshness_active_playlist_name);
}

/** Free all memory owned by the cache. */
void
cli_cache_free (cli_cache_t *cache)
{
	if (cache->conn != NULL)
		xmmsc_unref (cache->conn);
	g_free (cache->active_playlist_name);
	xmmsv_unref (cache->active_playlist);
	g_free (cache);
}
