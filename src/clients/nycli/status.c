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

static gchar *format_time (gint duration);

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
			playback = g_strdup (_("Stopped"));
			break;
		case XMMS_PLAYBACK_STATUS_PLAY:
			playback = g_strdup (_("Playing"));
			break;
		case XMMS_PLAYBACK_STATUS_PAUSE:
			playback = g_strdup (_("Paused"));
			break;
		default:
			playback = g_strdup (_("Unknown"));
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
	gint i;

	const gchar *time_fields[] = { "duration", NULL };
	const gchar *noinfo_fields[] = { "playback_status", "playtime", NULL };

	currid = g_array_index (infos->cache->active_playlist, guint,
	                        infos->cache->currpos);

	res = xmmsc_medialib_get_info (infos->sync, currid);
	xmmsc_result_wait (res);

	if (!xmmsc_result_iserror (res)) {
		GList *it;

		for (it = g_list_first (entry->format);
		     it != NULL;
		     it = g_list_next (it)) {

			gint ival;
			gchar *value, *field;
			const gchar *sval;
			xmmsc_result_value_type_t type;

			field = (gchar *)it->data + 2;

			for (i = 0; noinfo_fields[i] != NULL; i++) {
				if (!strcmp (field, noinfo_fields[i])) {
					goto not_info_field;
				}
			}

			type = xmmsc_result_get_dict_entry_type (res, field);
			switch (type) {
			case XMMSC_RESULT_VALUE_TYPE_STRING:
				xmmsc_result_get_dict_entry_string (res, field, &sval);
				value = g_strdup (sval);
				break;

			case XMMSC_RESULT_VALUE_TYPE_INT32:
				xmmsc_result_get_dict_entry_int (res, field, &ival);

				for (i = 0; time_fields[i] != NULL; i++) {
					if (!strcmp (time_fields[i], field)) {
						break;
					}
				}

				if (time_fields[i] != NULL) {
					value = format_time (ival);
				} else {
					value = g_strdup_printf ("%d", ival);
				}
				break;

			default:
				value = g_strdup (_("invalid field"));
			}

			/* FIXME: work with undefined fileds! pll parser? */
			g_hash_table_insert (entry->data, field, value);

		    not_info_field:
			;
		}

	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static void
status_update_playtime (cli_infos_t *infos, status_entry_t *entry)
{
	xmmsc_result_t *res;
	guint playtime;

	res = xmmsc_playback_playtime (infos->sync);
	xmmsc_result_wait (res);

	if (!xmmsc_result_iserror (res)) {
		xmmsc_result_get_uint (res, &playtime);
		g_hash_table_insert (entry->data, "playtime", format_time (playtime));
	} else {
		g_printf (_("Server error: %s\n"), xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

/* Returned string must be freed by the caller */
static gchar *
format_time (gint duration)
{
	gint min, sec;

	/* +500 for rounding */
	sec = (duration+500) / 1000;
	min = sec / 60;
	sec = sec % 60;

	return g_strdup_printf ("%02d:%02d", min, sec);
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
status_update_all (cli_infos_t *infos, status_entry_t *entry)
{
	status_update_playback (infos, entry);
	status_update_info (infos, entry);
	status_update_playtime (infos, entry);
}

void
status_print_entry (status_entry_t *entry)
{
	GList *it;
	gint rows, columns, currlen;

	readline_screen_size (&rows, &columns);

	currlen = 0;
	g_printf ("\r");
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
