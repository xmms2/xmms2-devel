/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
	xmmsv_t *val;
	xmmsv_t *pt;
	gint32 status;
	const gchar *playback;
	const gchar *err;

	res = xmmsc_playback_status (infos->sync);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_get_int (val, &status);

		switch (status) {
		case XMMS_PLAYBACK_STATUS_STOP:
			playback = _("Stopped");
			break;
		case XMMS_PLAYBACK_STATUS_PLAY:
			playback = _("Playing");
			break;
		case XMMS_PLAYBACK_STATUS_PAUSE:
			playback = _("Paused");
			break;
		default:
			playback = _("Unknown");
		}

		pt = xmmsv_new_string (playback);
		xmmsv_dict_set (entry->data, "playback_status", pt);
		xmmsv_unref (pt);

	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	xmmsc_result_unref (res);
}

static void
status_update_info (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	guint currid;
	gint i;

	const gchar *time_fields[] = { "duration"};
	const gchar *noinfo_fields[] = { "playback_status", "playtime", "position"};
	const gchar *err;

	currid = infos->cache->currid;
	/* Don't bother if it's 0 */
	if (!currid) {
		return;
	}

	res = xmmsc_medialib_get_info (infos->sync, currid);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_t *info;

		info = xmmsv_propdict_to_dict (val, NULL);
		enrich_mediainfo (info);

		/* copy over fields that are not from metadata */
		for (i = 0; i < G_N_ELEMENTS (noinfo_fields); i++) {
			xmmsv_t *copy;
			if (xmmsv_dict_get (entry->data, noinfo_fields[i], &copy)) {
				xmmsv_dict_set (info, noinfo_fields[i], copy);
			}
		}

		/* pretty format time fields */
		for (i = 0; i < G_N_ELEMENTS (time_fields); i++) {
			gint32 tim;
			if (xmmsv_dict_entry_get_int (info, time_fields[i], &tim)) {
				gchar *p;
				xmmsv_t *pt;
				p = format_time (tim, FALSE);
				pt = xmmsv_new_string (p);
				xmmsv_dict_set (info, time_fields[i], pt);
				xmmsv_unref (pt);
				g_free (p);
			}
		}
		xmmsv_unref (entry->data);
		entry->data = info;
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	xmmsc_result_unref (res);
}

static void
status_update_playtime (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	gint32 playtime;
	const gchar *err;

	res = xmmsc_playback_playtime (infos->sync);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_get_int (val, &playtime);
		gchar *p;
		xmmsv_t *pt;
		p = format_time (playtime, FALSE);
		pt = xmmsv_new_string (p);
		xmmsv_dict_set (entry->data, "playtime", pt);
		xmmsv_unref (pt);
		g_free (p);
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	xmmsc_result_unref (res);
}

static void
status_update_position (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsv_t *p;
	p = xmmsv_new_int (infos->cache->currpos);
	xmmsv_dict_set (entry->data, "position", p);
	xmmsv_unref (p);
}

static GList *
parse_format (const gchar *format)
{
	const gchar *s, *last;
	GList *strings = NULL;

	last = format;
	while ((s = strstr (last, "${")) != NULL) {
		/* Copy the substring before the variable */
		if (last != s) {
			strings = g_list_prepend (strings, g_strndup (last, s - last));
		}

		last = strchr (s, '}');
		if (last) {
			/* Copy the variable (as "${variable}") */
			strings = g_list_prepend (strings, g_strndup (s, last - s));
			last++;
		} else {
			/* Missing '}', keep '$' as a string and keep going */
			strings = g_list_prepend (strings, g_strndup (s, 1));
			last = s + 1;
		}
	}

	/* Copy the remaining substring after the last variable */
	if (*last != '\0') {
		strings = g_list_prepend (strings, g_strdup (last));
	}

	strings = g_list_reverse (strings);

	return strings;
}

status_entry_t *
status_init (gchar *format, gint refresh)
{
	status_entry_t *entry;

	entry = g_new0 (status_entry_t, 1);

	entry->data = xmmsv_new_dict ();
	entry->format = g_strdup (format);
	entry->refresh = refresh;

	return entry;
}

void
status_free (status_entry_t *entry)
{
	g_free (entry->format);
	xmmsv_unref (entry->data);
	g_free (entry);
}

void
status_update_all (cli_infos_t *infos, status_entry_t *entry)
{
	status_update_playback (infos, entry);
	status_update_position (infos, entry);
	status_update_info (infos, entry);
	status_update_playtime (infos, entry);
}

void
status_print_entry (status_entry_t *entry)
{
	gint columns, res;
	gchar *r;

	columns = find_terminal_width ();

	r = g_malloc (columns + 1);

	res = xmmsv_dict_format (r, columns + 1, entry->format, entry->data);

	g_printf ("%s", r);

	for (;res < columns; res++) {
		g_printf (" ");
	}

	fflush (stdout);

	g_free (r);
}
