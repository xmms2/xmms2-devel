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

#include "cmd_playback.h"
#include "common.h"


static void
do_reljump (xmmsc_connection_t *conn, gint where)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	res = xmmsc_playlist_set_next_rel (conn, where);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't advance in playlist: %s", errmsg);
	}
	xmmsc_result_unref (res);

	res = xmmsc_playback_tickle (conn);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
}


void
cmd_toggleplay (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	int32_t status;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	res = xmmsc_playback_status (conn);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't get playback status: %s", errmsg);
	}

	if (!xmmsv_get_int (val, &status)) {
		print_error ("Broken resultset");
	}

	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		cmd_pause (conn, argc, argv);
	} else {
		cmd_play (conn, argc, argv);
	}

	xmmsc_result_unref (res);
}


void
cmd_play (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	res = xmmsc_playback_start (conn);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't start playback: %s", errmsg);
	}
	xmmsc_result_unref (res);
}

void
cmd_stop (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	res = xmmsc_playback_stop (conn);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't stop playback: %s", errmsg);
	}
	xmmsc_result_unref (res);
}


void
cmd_pause (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	res = xmmsc_playback_pause (conn);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't pause playback: %s", errmsg);
	}
	xmmsc_result_unref (res);
}


void
cmd_next (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	do_reljump (conn, 1);
}


void
cmd_prev (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	do_reljump (conn, -1);
}


void
cmd_seek (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	long arg;
	gchar *endptr = NULL;

	if (argc < 3) {
		print_error ("You need to specify a number of seconds. Usage:\n"
		             "xmms2 seek n  - seek to absolute position in song\n"
		             "xmms2 seek +n - seek n seconds forward in song\n"
		             "xmms2 seek -n - seek n seconds backwards");
	}

	/* parse the movement argument */
	arg = strtol (argv[2], &endptr, 10) * 1000;

	if (endptr == argv[2]) {
		print_error ("invalid argument");
	}

	if (argv[2][0] == '+' || argv[2][0] == '-') {
		/* do we need to seek at all? */
		if (!arg) {
			return;
		}
		res = xmmsc_playback_seek_ms_rel (conn, arg);
	} else {
		res = xmmsc_playback_seek_ms (conn, arg);
	}

	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't seek to %d arg: %s", arg, errmsg);
	}
	xmmsc_result_unref (res);
}


void
cmd_jump (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc < 3) {
		print_error ("You'll need to specify a position to jump to. Usage:\n"
		             "xmms2 jump n  - jump to absolute position n\n"
		             "xmms2 jump +n - advance n positions\n"
		             "xmms2 jump -n - jump n positions backwards\n");
	}

	if (argv[2][0] == '-' || argv[2][0] == '+') {
		res = xmmsc_playlist_set_next_rel (conn, strtol (argv[2], NULL, 10));
	} else {
		res = xmmsc_playlist_set_next (conn, strtol (argv[2], NULL, 10));
	}
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't jump to that song: %s", errmsg);
	}
	xmmsc_result_unref (res);

	res = xmmsc_playback_tickle (conn);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't go to next song: %s", errmsg);
	}
	xmmsc_result_unref (res);
}
