/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include "cmd_status.h"
#include "common.h"


/**
 * Function prototypes
 */
static int handle_current_id (xmmsv_t *res, void *userdata);
static int handle_playtime (xmmsv_t *res, void *userdata);
static int handle_mediainfo_update (xmmsv_t *res, void *userdata);
static int handle_status_change (xmmsv_t *res, void *userdata);
static int do_mediainfo (xmmsv_t *res, void *userdata);
static void update_display ();
static void quit (void *data);


/**
 * Globals
 */
extern gchar *statusformat;

static gboolean has_songname = FALSE;
static gboolean fetching_songname = FALSE;
static gint current_id = 0;
static gint last_dur = 0;
static gint curr_dur = 0;
static gchar songname[256];
static gint curr_status = 0;

static const gchar *status_messages[] = {
	"Stopped",
	"Playing",
	"Paused"
};

/**
 * Functions
 */
void
cmd_status (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	GMainLoop *ml;

	ml = g_main_loop_new (NULL, FALSE);

	has_songname = FALSE;

	/* Setup onchange signal for mediainfo */
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_playback_current_id,
	                   handle_current_id, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_signal_playback_playtime,
	                   handle_playtime, NULL);
	XMMS_CALLBACK_SET (conn, xmmsc_playback_current_id,
	                   handle_current_id, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_medialib_entry_changed,
	                   handle_mediainfo_update, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_playback_status,
	                   handle_status_change, NULL);
	XMMS_CALLBACK_SET (conn, xmmsc_playback_status,
	                   handle_status_change, NULL);

	xmmsc_disconnect_callback_set (conn, quit, NULL);
	xmmsc_mainloop_gmain_init (conn);

	g_main_loop_run (ml);
}


void
cmd_current (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *propdict, *val;
	gchar print_text[256];
	gint id;

	res = xmmsc_playback_current_id (conn);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	if (!xmmsv_get_int (val, &id)) {
		print_error ("Broken resultset");
	}
	xmmsc_result_unref (res);

	/* no current track, abort */
	if (id == 0) {
		print_info ("");
		return;
	}

	res = xmmsc_medialib_get_info (conn, id);
	xmmsc_result_wait (res);
	propdict = xmmsc_result_get_value (res);
	val = xmmsv_propdict_to_dict (propdict, NULL);

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	if (argc > 2) {
		xmmsc_entry_format (print_text, sizeof (print_text), argv[2], val);
	} else {
		xmmsc_entry_format (print_text, sizeof (print_text),
		                    "${artist} - ${title}", val);
	}

	xmmsv_unref (val);

	print_info ("%s", print_text);
	xmmsc_result_unref (res);
}

static int
handle_status_change (xmmsv_t *val, void *userdata)
{
	gint new_status;

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	if (!xmmsv_get_int (val, &new_status)) {
		print_error ("Broken resultset");
	}

	curr_status = new_status;
	update_display ();

	return TRUE;
}

static int
handle_current_id (xmmsv_t *val, void *userdata)
{
	xmmsc_result_t *res;
	xmmsc_connection_t *conn = userdata;

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	if (!xmmsv_get_int (val, &current_id)) {
		print_error ("Broken resultset");
	}

	if (current_id) {
		fetching_songname = TRUE;
		res = xmmsc_medialib_get_info (conn, current_id);
		xmmsc_result_notifier_set (res, do_mediainfo, NULL);
		xmmsc_result_unref (res);
	}

	return TRUE;
}


static int
handle_playtime (xmmsv_t *val, void *userdata)
{
	gint dur;

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	if (!xmmsv_get_int (val, &dur)) {
		print_error ("Broken resultset");
	}

	if (((dur / 1000) % 60) != ((last_dur / 1000) % 60)) {
		last_dur = dur;

		if (!fetching_songname)
			update_display ();

	}

	/* Trigger restart */
	return TRUE;
}

static void update_display ()
{
	gchar *conv;
	gsize r, w;
	GError *err = NULL;

	if (has_songname) {
		gchar buf_status[32];
		gchar buf_time[32];
		gint room, len, columns;

		columns = find_terminal_width ();

		g_snprintf (buf_status, sizeof (buf_status), "%7s: ",
		            status_messages[curr_status]);
		g_snprintf (buf_time, sizeof (buf_time), ": %02d:%02d of %02d:%02d",
		            last_dur / 60000, (last_dur / 1000) % 60, curr_dur / 60000,
		            (curr_dur / 1000) % 60);

		room = columns - strlen (buf_status) - strlen (buf_time);

		len = g_utf8_strlen (songname, -1);
		if (room >= len || room <= 4) { /* don't even try.. */
			conv =  g_locale_from_utf8 (songname, -1, &r, &w, &err);
			printf ("\r%s%s%s", buf_status, conv, buf_time);
		} else {
			gint t = g_utf8_offset_to_pointer (songname, room - 3) - songname;

			conv =  g_locale_from_utf8 (songname, t, &r, &w, &err);
			printf ("\r%s%s...%s", buf_status, conv, buf_time);
		}
		g_free (conv);

		fflush (stdout);
	}
}

static int
handle_mediainfo_update (xmmsv_t *val, void *userdata)
{
	gint id;
	xmmsc_result_t *res;
	xmmsc_connection_t *conn = userdata;

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	if (!xmmsv_get_int (val, &id)) {
		print_error ("Broken resultset");
	}

	if (id == current_id) {
		res = xmmsc_medialib_get_info (conn, current_id);
		xmmsc_result_notifier_set (res, do_mediainfo, NULL);
		xmmsc_result_unref (res);
	}

	return TRUE;
}


static int
do_mediainfo (xmmsv_t *propdict, void *userdata)
{
	xmmsv_t *val;

	if (xmmsv_is_error (propdict)) {
		print_error ("%s", xmmsv_get_error_old (propdict));
	}

	val = xmmsv_propdict_to_dict (propdict, NULL);

	print_info ("");
	if (xmmsv_dict_has_key (val, "channel") && xmmsv_dict_has_key (val, "title")) {
		xmmsc_entry_format (songname, sizeof (songname),
		                    "[stream] ${title}", val);
		has_songname = TRUE;
	} else if (xmmsv_dict_has_key (val, "channel")) {
		xmmsc_entry_format (songname, sizeof (songname), "${channel}", val);
		has_songname = TRUE;
	} else if (!xmmsv_dict_has_key (val, "title")) {
		const gchar *url;

		if (xmmsv_dict_entry_get_string (val, "url", &url)) {
			gchar *filename = g_path_get_basename (url);

			if (filename) {
				g_snprintf (songname, sizeof (songname), "%s", filename);
				g_free (filename);
				has_songname = TRUE;
			}
		}
	} else {
		xmmsc_entry_format (songname, sizeof (songname),
		                    statusformat, val);
		has_songname = TRUE;
	}

	if (xmmsv_dict_entry_get_int (val, "duration", &curr_dur)) {
		/* rounding */
		curr_dur += 500;
	} else {
		curr_dur = 0;
	}

	xmmsv_unref (val);

	fetching_songname = FALSE;

	return FALSE;
}


static void
quit (void *data)
{
	print_info ("\nbye cruel world!");
	exit (EXIT_SUCCESS);
}
