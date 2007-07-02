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


static
void unref_transient_result (xmmsc_result_t *res)
{
	if (xmmsc_result_get_class (res) != XMMSC_RESULT_CLASS_BROADCAST) {
		xmmsc_result_unref (res);
	}
}

static
void refresh_uint (xmmsc_result_t *res, void *udata)
{
	guint *v = (guint *) udata;

	if (!xmmsc_result_iserror (res)) {
		xmmsc_result_get_uint (res, v);
	}

	unref_transient_result (res);
}

static
void refresh_string (xmmsc_result_t *res, void *udata)
{
	gchar **v = (gchar **) udata;
	gchar *buf;

	if (!xmmsc_result_iserror (res) && xmmsc_result_get_string (res, &buf)) {
		if (*v) {
			g_free (*v);
		}
		*v = g_strdup (buf);
	}

	unref_transient_result (res);
}

static
void refresh_active_playlist (xmmsc_result_t *res, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;
	guint id;

	if (!xmmsc_result_iserror (res)) {
		/* Reset array */
		if (cache->active_playlist_size > 0) {
			gint end_index = cache->active_playlist_size - 1;
			cache->active_playlist = g_array_remove_range (cache->active_playlist,
			                                               0, end_index);
			cache->active_playlist_size = 0;
		}

		/* .. and refill it */
		while (xmmsc_result_list_valid (res)) {
			if (!xmmsc_result_get_uint (res, &id)) {
				/* FIXME: what happens on error? */
				continue;
			}
			g_array_append_val (cache->active_playlist, id);

			xmmsc_result_list_next (res);
			cache->active_playlist_size++;
		}
	}

	unref_transient_result (res);
}

static
void update_active_playlist (xmmsc_result_t *res, void *udata)
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
		cache->active_playlist_size++;
		break;

	case XMMS_PLAYLIST_CHANGED_INSERT:
		g_array_insert_val (cache->active_playlist, pos, id);
		cache->active_playlist_size++;
		break;

	case XMMS_PLAYLIST_CHANGED_MOVE:
		xmmsc_result_get_dict_entry_int (res, "newposition", &newpos);
		g_array_remove_index (cache->active_playlist, pos);
		g_array_insert_val (cache->active_playlist, newpos, id);
		break;

	case XMMS_PLAYLIST_CHANGED_REMOVE:
		g_array_remove_index (cache->active_playlist, pos);
		cache->active_playlist_size--;
		break;

	case XMMS_PLAYLIST_CHANGED_SHUFFLE:
	case XMMS_PLAYLIST_CHANGED_SORT:
	case XMMS_PLAYLIST_CHANGED_CLEAR:
		/* Oops, reload the whole playlist */
		/* FIXME: the cache is dirty until it's refreshed.. ! */
		refres = xmmsc_playlist_list_entries (infos->conn, XMMS_ACTIVE_PLAYLIST);
		xmmsc_result_notifier_set (refres, &refresh_active_playlist, infos->cache);
		xmmsc_result_unref (refres);
		break;
	}

	unref_transient_result (res);
}

static
void reload_active_playlist (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	xmmsc_result_t *refres;
	gchar *buf;

	/* Refresh playlist name */
	if (xmmsc_result_get_string (res, &buf)) {
		if (infos->cache->active_playlist_name) {
			g_free (infos->cache->active_playlist_name);
		}
		infos->cache->active_playlist_name = g_strdup (buf);
	}

	/* Get all the entries again */
	/* FIXME: the cache is dirty until it's refreshed.. ! */
	refres = xmmsc_playlist_list_entries (infos->conn, XMMS_ACTIVE_PLAYLIST);
	xmmsc_result_notifier_set (refres, &refresh_active_playlist, infos->cache);
	xmmsc_result_unref (refres);

	unref_transient_result (res);
}

cli_cache_t *
cli_cache_init ()
{
	cli_cache_t *cache;

	cache = g_new0 (cli_cache_t, 1);
	cache->active_playlist = g_array_new (FALSE, TRUE, sizeof (guint));

	return cache;
}

void
cli_cache_start (cli_infos_t *infos)
{
	xmmsc_result_t *res;

	/* FIXME: Possible race condition if cache accessed before value
	 *        has arrived? workaround: set a flag until the cache is
	 *        filled. */

	/* Setup one-time value fetchers, for init */
	res = xmmsc_playlist_current_pos (infos->conn, XMMS_ACTIVE_PLAYLIST);
	xmmsc_result_notifier_set (res, &refresh_uint, &infos->cache->currpos);
	xmmsc_result_unref (res);

	res = xmmsc_playlist_list_entries (infos->conn, XMMS_ACTIVE_PLAYLIST);
	xmmsc_result_notifier_set (res, &refresh_active_playlist, infos->cache);
	xmmsc_result_unref (res);

	res = xmmsc_playlist_current_active (infos->conn);
	xmmsc_result_notifier_set (res, &refresh_string,
	                           &infos->cache->active_playlist_name);
	xmmsc_result_unref (res);

	/* Setup async listeners */
	res = xmmsc_broadcast_playlist_current_pos (infos->conn);
	xmmsc_result_notifier_set (res, &refresh_uint, &infos->cache->currpos);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playlist_changed (infos->conn);
	xmmsc_result_notifier_set (res, &update_active_playlist, infos);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playlist_loaded (infos->conn);
	xmmsc_result_notifier_set (res, &reload_active_playlist, infos);
	xmmsc_result_unref (res);
}

void
cli_cache_free (cli_cache_t *cache)
{
	if (cache->active_playlist_name) {
		g_free (cache->active_playlist_name);
	}
	g_array_free (cache->active_playlist, FALSE);
	g_free (cache);
}
