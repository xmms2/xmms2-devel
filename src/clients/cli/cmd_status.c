/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include "common.h"


/**
 * Function prototypes
 */
static void handle_current_id (xmmsc_result_t *res, void *userdata);
static void handle_playtime (xmmsc_result_t *res, void *userdata);
static void handle_mediainfo_update (xmmsc_result_t *res, void *userdata);
static void do_mediainfo (xmmsc_result_t *res, void *userdata);
static void quit (void *data);


/**
 * Globals
 */
extern gchar *statusformat;

static gboolean has_songname = FALSE;
static guint current_id = 0;
static guint last_dur = 0;
static gint curr_dur = 0;
static gchar songname[60];


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

	xmmsc_disconnect_callback_set (conn, quit, NULL);
	xmmsc_mainloop_gmain_init (conn);

	g_main_loop_run (ml);
}


void
cmd_current (xmmsc_connection_t *conn, gint argc, gchar **argv)
{ 
	xmmsc_result_t *res;
	gchar print_text[256];
	guint id;

	res = xmmsc_playback_current_id (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	if (!xmmsc_result_get_uint (res, &id)) {
		print_error ("Broken resultset");
	}
	xmmsc_result_unref (res);

	res = xmmsc_medialib_get_info (conn, id);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	if (argc > 2) {
		xmmsc_entry_format (print_text, sizeof(print_text), argv[2], res);	
	} else {
		xmmsc_entry_format (print_text, sizeof(print_text), 
                            "${artist} - ${title}", res);
	}

	print_info ("%s", print_text);
	xmmsc_result_unref (res);
}


static void
handle_current_id (xmmsc_result_t *res, void *userdata)
{
	xmmsc_connection_t *conn = userdata;

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	if (!xmmsc_result_get_uint (res, &current_id)) {
		print_error ("Broken resultset");
	}

	res = xmmsc_medialib_get_info (conn, current_id);
	xmmsc_result_notifier_set (res, do_mediainfo, NULL);
	xmmsc_result_unref (res);
}


static void
handle_playtime (xmmsc_result_t *res, void *userdata)
{
	xmmsc_result_t *newres;
	GError *err = NULL;
	guint dur;
	gsize r, w;
	gchar *conv;

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	
	if (!xmmsc_result_get_uint (res, &dur)) {
		print_error ("Broken resultset");
	}

	if (has_songname && ((dur / 1000) % 60) != ((last_dur / 1000) % 60)) {

		last_dur = dur;

		conv =  g_locale_from_utf8 (songname, -1, &r, &w, &err);
		printf ("\rPlaying: %s: %02d:%02d of %02d:%02d", conv,
		        dur / 60000, (dur / 1000) % 60, curr_dur / 60000,
		        (curr_dur / 1000) % 60);
		g_free (conv);

		fflush (stdout);

	}
	newres = xmmsc_result_restart (res);
	xmmsc_result_unref (res);
	xmmsc_result_unref (newres);
}


static void
handle_mediainfo_update (xmmsc_result_t *res, void *userdata)
{
	guint id;
	xmmsc_connection_t *conn = userdata;

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	if (!xmmsc_result_get_uint (res, &id)) {
		print_error ("Broken resultset");
	}
	xmmsc_result_unref (res);

	if (id == current_id) {
		res = xmmsc_medialib_get_info (conn, current_id);
		xmmsc_result_notifier_set (res, do_mediainfo, NULL);
		xmmsc_result_unref (res);
	}
}


static void
do_mediainfo (xmmsc_result_t *res, void *userdata)
{
	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	print_info ("");
	if (res_has_key (res, "channel") && res_has_key (res, "title")) {
		xmmsc_entry_format (songname, sizeof (songname),
		                    "[stream] ${title}", res);
		has_songname = TRUE;
	} else if (res_has_key (res, "channel")) {
		xmmsc_entry_format (songname, sizeof (songname), "${title}", res);
		has_songname = TRUE;
	} else if (!res_has_key (res, "title")) {
		gchar *url, *filename;

		if (xmmsc_result_get_dict_entry_str (res, "url", &url)) {
			filename = g_path_get_basename (url);

			if (filename) {
				g_snprintf (songname, sizeof (songname), "%s", filename);
				g_free (filename);
				has_songname = TRUE;
			}
		}
	} else {
		xmmsc_entry_format (songname, sizeof (songname),
		                    statusformat, res);
		has_songname = TRUE;
	}

	xmmsc_result_get_dict_entry_int32 (res, "duration", &curr_dur);

	xmmsc_result_unref (res);
}


static void
quit (void *data)
{
	print_info ("\nbye cruel world!");
	exit (0);
}
