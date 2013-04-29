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

#include <xmmsclient/xmmsclient.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "main.h"
#include "status.h"
#include "currently_playing.h"
#include "utils.h"
#include "readline.h"
#include "cli_cache.h"
#include "cli_infos.h"
#include "compat.h"
#include "xmmscall.h"

struct currently_playing_St {
	xmmsv_t *data;
	gchar *format;
};
typedef struct currently_playing_St currently_playing_t;

static void
currently_playing_update_status (currently_playing_t *entry, xmmsv_t *value)
{
	const gchar *status_name;
	gint status = -1;

	xmmsv_get_int (value, &status);

	switch (status) {
		case XMMS_PLAYBACK_STATUS_STOP:
			status_name = _("Stopped");
			break;
		case XMMS_PLAYBACK_STATUS_PLAY:
			status_name = _("Playing");
			break;
		case XMMS_PLAYBACK_STATUS_PAUSE:
			status_name = _("Paused");
			break;
		default:
			status_name = _("Unknown");
			break;
	}

	xmmsv_dict_set_string (entry->data, "playback_status", status_name);
}

static void
currently_playing_request_status (cli_infos_t *infos, currently_playing_t *entry)
{
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playback_status, cli_infos_xmms_sync (infos)),
	                 FUNC_CALL_P (currently_playing_update_status, entry, XMMS_PREV_VALUE));
}

static void
currently_playing_update_info (currently_playing_t *entry, xmmsv_t *value)
{
	const gchar *noinfo_fields[] = { "playback_status", "playtime", "position"};
	const gchar *time_fields[] = { "duration"};
	xmmsv_t *info;
	gint i;

	info = xmmsv_propdict_to_dict (value, NULL);

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
			gchar *p = format_time (tim, FALSE);
			xmmsv_dict_set_string (info, time_fields[i], p);
			g_free (p);
		}
	}

	xmmsv_unref (entry->data);
	entry->data = info;
}

static void
currently_playing_request_info (cli_infos_t *infos, currently_playing_t *entry)
{
	if (cli_infos_current_id (infos) > 0) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, cli_infos_xmms_sync (infos), cli_infos_current_id (infos)),
		                 FUNC_CALL_P (currently_playing_update_info, entry, XMMS_PREV_VALUE));
	}
}

static void
currently_playing_update_playtime (currently_playing_t *entry, xmmsv_t *value)
{
	gchar *formatted;
	gint playtime;

	xmmsv_get_int (value, &playtime);

	formatted = format_time (playtime, FALSE);
	xmmsv_dict_set_string (entry->data, "playtime", formatted);
	g_free (formatted);
}

static void
currently_playing_request_playtime (cli_infos_t *infos, currently_playing_t *entry)
{
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playback_playtime, cli_infos_xmms_sync (infos)),
	                 FUNC_CALL_P (currently_playing_update_playtime, entry, XMMS_PREV_VALUE));
}

static void
currently_playing_update_position (cli_infos_t *infos,
                                   currently_playing_t *entry)
{
	xmmsv_dict_set_int (entry->data, "position", cli_infos_current_position (infos));
}

static void
currently_playing_free (gpointer udata)
{
	currently_playing_t *entry = (currently_playing_t *)udata;

	g_free (entry->format);
	xmmsv_unref (entry->data);
	g_free (entry);
}

static void
currently_playing_update_all (cli_infos_t *infos, currently_playing_t *entry)
{
	currently_playing_request_status (infos, entry);
	currently_playing_update_position (infos, entry);
	currently_playing_request_info (infos, entry);
	currently_playing_request_playtime (infos, entry);
}

static void
currently_playing_print_entry (currently_playing_t *entry)
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

	g_free (r);
}

static void
currently_playing_refresh (cli_infos_t *infos, gpointer udata,
                           gboolean first, gboolean last) {
	currently_playing_t *entry = (currently_playing_t *)udata;

	if (first || !last) {
		currently_playing_update_all (infos, entry);
	}

	if (!first) { g_printf ("\r"); }
	currently_playing_print_entry (entry);
	if (last) { g_printf ("\n"); }

	fflush (stdout);
}

static const keymap_entry_t currently_playing_keymap[] = {
	{"n"    , "n"       , "next-song"       , _("jump to next song")},
	{"p"    , "p"       , "previous-song"   , _("jump to previous song")},
	{" "    , _("SPACE"), "toggle-playback" , _("toggle playback")},
	{"\\C-j", _("ENTER"), "quit-status-mode", _("exit status mode")},
	{"\\C-m", NULL      , "quit-status-mode", NULL},
	{NULL, NULL, NULL, NULL}
};

status_entry_t *
currently_playing_init (const gchar *format, gint refresh)
{
	currently_playing_t *entry;

	entry = g_new0 (currently_playing_t, 1);
	entry->data = xmmsv_new_dict ();
	entry->format = g_strdup (format);

	return status_init (currently_playing_free,
	                    currently_playing_refresh,
	                    NULL,
	                    currently_playing_keymap,
	                    entry, refresh);
}
