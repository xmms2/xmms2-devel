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

#include <xmmsclient/xmmsclient.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "main.h"
#include "status.h"
#include "utils.h"
#include "readline.h"
#include "cli_cache.h"
#include "cli_infos.h"

static void
status_update_playback (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	guint status;
	gchar *playback;

	res = xmmsc_playback_status (infos->sync);
	xmmsc_result_wait (res);

	if (!xmmsc_result_iserror (res)) {
		xmmsc_result_get_uint (res, &status);

		switch (status) {
		case XMMS_PLAYBACK_STATUS_STOP:
			playback = g_strdup ("Stopped");
			break;
		case XMMS_PLAYBACK_STATUS_PLAY:
			playback = g_strdup ("Playing");
			break;
		case XMMS_PLAYBACK_STATUS_PAUSE:
			playback = g_strdup ("Paused");
			break;
		default:
			playback = g_strdup ("Unknown");
		}

		g_hash_table_insert (entry->data, "playback_status", playback);

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static void
status_update_info (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	guint currid;
	gint duration, min, sec;
	const gchar *title, *artist;

	currid = g_array_index (infos->cache->active_playlist, guint,
	                        infos->cache->currpos);

	res = xmmsc_medialib_get_info (infos->sync, currid);
	xmmsc_result_wait (res);

	/* FIXME: use xmmsc_entry_format ? */
	if (!xmmsc_result_iserror (res)) {
		xmmsc_result_get_dict_entry_string (res, "title", &title);
		xmmsc_result_get_dict_entry_string (res, "artist", &artist);

		/* + 500 for rounding */
		xmmsc_result_get_dict_entry_int (res, "duration", &duration);
		sec = (duration+500) / 1000;
		min = sec / 60;
		sec = sec % 60;

		g_hash_table_insert (entry->data, "title", g_strdup (title));
		g_hash_table_insert (entry->data, "artist", g_strdup (artist));
		g_hash_table_insert (entry->data, "duration",
		                     g_strdup_printf ("%02d:%02d", min, sec));
	}

	xmmsc_result_unref (res);
}

static void
status_update_playtime (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	guint playtime, min, sec;
	gchar *formated;

	res = xmmsc_playback_playtime (infos->sync);
	xmmsc_result_wait (res);

	if (!xmmsc_result_iserror (res)) {
		xmmsc_result_get_uint (res, &playtime);
		
		/* + 500 for rounding */
		sec = (playtime+500) / 1000;
		min = sec / 60;
		sec = sec % 60;

		formated = g_strdup_printf ("%02d:%02d", min, sec);

		g_hash_table_insert (entry->data, "playtime", formated);

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static GList *
parse_format (gchar *format) {
	gchar *copy, *s, *last;
	GList *strings = NULL;

	copy = g_strdup (format);

	last = copy;
	while ((s = strstr(last, "${")) != NULL) {
		if (last != s) {
			*s = '\0';
			strings = g_list_prepend (strings, g_strdup (last));
			*s = '$';
		}

		last = strchr (s, '}');
		*last = '\0';
		strings = g_list_prepend (strings, g_strdup (s));

		last++;
	}
	strings = g_list_reverse (strings);

	g_free (copy);

	return strings;
}

status_entry_t *
status_init (gchar *format, gint refresh)
{
	status_entry_t *entry;

	entry = g_new0 (status_entry_t, 1);

	entry->data = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
	entry->format = parse_format (format);
	entry->refresh = refresh;

	return entry;
}

void
status_free (status_entry_t *entry)
{
	GList *it;

	for (it = g_list_first (entry->format); it != NULL; it = g_list_next (it)) {
		g_free (it->data);
	}
	g_list_free (entry->format);

	g_hash_table_destroy (entry->data);
	g_free (entry);
}

void
status_update_all (cli_infos_t *infos, status_entry_t *entry) {
	status_update_playback (infos, entry);
	status_update_info (infos, entry);
	status_update_playtime (infos, entry);
}

void
status_set_next_rel (cli_infos_t *infos, gint offset)
{
	xmmsc_result_t *res;

	res = xmmsc_playlist_set_next_rel (infos->sync, offset);
	xmmsc_result_wait (res);
	tickle (res, infos);
}

void
status_print_entry (status_entry_t *entry)
{
	GList *it;
	gint rows, columns, currlen;

	readline_screen_size (&rows, &columns);

	currlen = 0;
	g_printf("\r");
	for (it = g_list_first (entry->format); it != NULL; it = g_list_next (it)) {
		gchar *s = it->data;
		if (s[0] == '$' && s[1] == '{') {
			s = g_hash_table_lookup (entry->data, s+2);
		}

		currlen += strlen (s);
		if (currlen >= columns) {
			break;
		} else {
			g_printf ("%s", s);
		}
	}

	while (currlen++ < columns) {
		g_printf (" ");
	}

	/* ${playback_status}: ${artist} - ${title}: ${playtime} of ${duration} */

	fflush (stdout);
}
