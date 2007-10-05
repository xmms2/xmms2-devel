/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "cli_cache.h"

#include "cli_infos.h"



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

static void
unref_transient_result (xmmsc_result_t *res)
{
	if (xmmsc_result_get_class (res) != XMMSC_RESULT_CLASS_BROADCAST) {
		xmmsc_result_unref (res);
	}
}

static void
refresh_currpos (xmmsc_result_t *res, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;

	if (!xmmsc_result_iserror (res)) {
		xmmsc_result_get_uint (res, &cache->currpos);
	}

	freshness_received (&cache->freshness_currpos);
	unref_transient_result (res);
}

static void
refresh_active_playlist_name (xmmsc_result_t *res, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;
	gchar *buf;

	if (!xmmsc_result_iserror (res) && xmmsc_result_get_string (res, &buf)) {
		g_free (cache->active_playlist_name);
		cache->active_playlist_name = g_strdup (buf);
	}

	freshness_received (&cache->freshness_active_playlist_name);
	unref_transient_result (res);
}

static void
refresh_active_playlist (xmmsc_result_t *res, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;
	guint id;

	if (!xmmsc_result_iserror (res)) {
		/* Reset array */
		if (cache->active_playlist->len > 0) {
			gint end_index = cache->active_playlist->len - 1;
			cache->active_playlist = g_array_remove_range (cache->active_playlist,
			                                               0, end_index);
		}

		/* .. and refill it */
		while (xmmsc_result_list_valid (res)) {
			xmmsc_result_get_uint (res, &id);
			g_array_append_val (cache->active_playlist, id);

			xmmsc_result_list_next (res);
		}
	}

	freshness_received (&cache->freshness_active_playlist);
	unref_transient_result (res);
}

static void
update_active_playlist (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	cli_cache_t *cache = infos->cache;
	xmmsc_result_t *refres;
	gint pos, newpos, type;
	guint id;
	gchar *name;

	xmmsc_result_get_dict_entry_int (res, "type", &type);
	xmmsc_result_get_dict_entry_int (res, "position", &pos);
	xmmsc_result_get_dict_entry_uint (res, "id", &id);
	xmmsc_result_get_dict_entry_string (res, "name", &name);

	/* Active playlist not changed, nevermind */
	if (strcmp (name, cache->active_playlist_name) != 0) {
		return;
	}

	/* Apply changes to the cached playlist */
	switch (type) {
	case XMMS_PLAYLIST_CHANGED_ADD:
		g_array_append_val (cache->active_playlist, id);
		break;

	case XMMS_PLAYLIST_CHANGED_INSERT:
		g_array_insert_val (cache->active_playlist, pos, id);
		break;

	case XMMS_PLAYLIST_CHANGED_MOVE:
		xmmsc_result_get_dict_entry_int (res, "newposition", &newpos);
		g_array_remove_index (cache->active_playlist, pos);
		g_array_insert_val (cache->active_playlist, newpos, id);
		break;

	case XMMS_PLAYLIST_CHANGED_REMOVE:
		g_array_remove_index (cache->active_playlist, pos);
		break;

	case XMMS_PLAYLIST_CHANGED_SHUFFLE:
	case XMMS_PLAYLIST_CHANGED_SORT:
	case XMMS_PLAYLIST_CHANGED_CLEAR:
		/* Oops, reload the whole playlist */
		refres = xmmsc_playlist_list_entries (infos->conn, XMMS_ACTIVE_PLAYLIST);
		xmmsc_result_notifier_set (refres, &refresh_active_playlist, infos->cache);
		xmmsc_result_unref (refres);
		freshness_requested (&cache->freshness_active_playlist);
		break;
	}

	unref_transient_result (res);
}

static void
reload_active_playlist (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	xmmsc_result_t *refres;
	gchar *buf;

	/* FIXME: Also listen to playlist renames, in case the active PL is renamed! */

	/* Refresh playlist name */
	if (xmmsc_result_get_string (res, &buf)) {
		g_free (infos->cache->active_playlist_name);
		infos->cache->active_playlist_name = g_strdup (buf);
	}

	/* Get all the entries again */
	refres = xmmsc_playlist_list_entries (infos->conn, XMMS_ACTIVE_PLAYLIST);
	xmmsc_result_notifier_set (refres, &refresh_active_playlist, infos->cache);
	xmmsc_result_unref (refres);
	freshness_requested (&infos->cache->freshness_active_playlist);

	unref_transient_result (res);
}

/** Initialize the cache, must still be started to be filled. */
cli_cache_t *
cli_cache_init ()
{
	cli_cache_t *cache;

	cache = g_new0 (cli_cache_t, 1);
	cache->currpos = 0;
	cache->active_playlist = g_array_new (FALSE, TRUE, sizeof (guint));
	cache->active_playlist_name = NULL;

	/* Init the freshness state */
	freshness_init (&cache->freshness_currpos);
	freshness_init (&cache->freshness_active_playlist);
	freshness_init (&cache->freshness_active_playlist_name);

	return cache;
}

/** Fill the cache with initial (current) data, setup listeners. */
void
cli_cache_start (cli_infos_t *infos)
{
	xmmsc_result_t *res;

	/* Setup one-time value fetchers, for init */
	res = xmmsc_playlist_current_pos (infos->conn, XMMS_ACTIVE_PLAYLIST);
	xmmsc_result_notifier_set (res, &refresh_currpos, infos->cache);
	xmmsc_result_unref (res);

	res = xmmsc_playlist_list_entries (infos->conn, XMMS_ACTIVE_PLAYLIST);
	xmmsc_result_notifier_set (res, &refresh_active_playlist, infos->cache);
	xmmsc_result_unref (res);

	res = xmmsc_playlist_current_active (infos->conn);
	xmmsc_result_notifier_set (res, &refresh_active_playlist_name,
	                           infos->cache);
	xmmsc_result_unref (res);

	/* Setup async listeners */
	res = xmmsc_broadcast_playlist_current_pos (infos->conn);
	xmmsc_result_notifier_set (res, &refresh_currpos, infos->cache);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playlist_changed (infos->conn);
	xmmsc_result_notifier_set (res, &update_active_playlist, infos);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playlist_loaded (infos->conn);
	xmmsc_result_notifier_set (res, &reload_active_playlist, infos);
	xmmsc_result_unref (res);
}

/** Check whether the cache is currently fresh (up-to-date). */
gboolean
cli_cache_is_fresh (cli_cache_t *cache)
{
	/* Check if all items are fresh */
	return freshness_is_fresh (&cache->freshness_currpos) &&
	       freshness_is_fresh (&cache->freshness_active_playlist) &&
	       freshness_is_fresh (&cache->freshness_active_playlist_name);
}

/** Free all memory owned by the cache. */
void
cli_cache_free (cli_cache_t *cache)
{
	g_free (cache->active_playlist_name);
	g_array_free (cache->active_playlist, FALSE);
	g_free (cache);
}
