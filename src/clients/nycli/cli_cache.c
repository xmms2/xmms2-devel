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
void refresh_uint (xmmsc_result_t *res, void *udata)
{
	guint *v = (guint *) udata;

	if (!xmmsc_result_iserror (res)) {
		xmmsc_result_get_uint (res, v);
	}
}

static
void refresh_active_playlist (xmmsc_result_t *res, void *udata)
{
	cli_cache_t *cache = (cli_cache_t *) udata;
	guint id;

	if (!xmmsc_result_iserror (res)) {
		cache->active_playlist_size = 0;
		while (xmmsc_result_list_valid (res)) {
			if (!xmmsc_result_get_uint (res, &id)) {
				/* FIXME: what happens on error?*/
			}
			g_array_append_val (cache->active_playlist, id);

			xmmsc_result_list_next (res);
			cache->active_playlist_size++;
		}
	}
}

static
void update_active_playlist (xmmsc_result_t *res, void *udata)
{
	/* FIXME: code this */
}

static
void reload_active_playlist (xmmsc_result_t *res, void *udata)
{
	/* FIXME: code this */
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

	/* Setup async listeners */
	res = xmmsc_broadcast_playlist_current_pos (infos->conn);
	xmmsc_result_notifier_set (res, &refresh_uint, &infos->cache->currpos);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playlist_changed (infos->conn);
	xmmsc_result_notifier_set (res, &update_active_playlist, infos->cache);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_playlist_loaded (infos->conn);
	xmmsc_result_notifier_set (res, &reload_active_playlist, infos);
	xmmsc_result_unref (res);
}

void
cli_cache_free (cli_cache_t *cache)
{
	/* FIXME: error? */
	g_array_free (cache->active_playlist, FALSE);
	g_free (cache);
}
