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




/** @file
 * Extremly simple example of a visualization for xmms2
 * (should probably just be put in /usr/share/doc/xmms2-dev/examples)
 */


#include <glib.h>
#include <math.h>
#include <stdlib.h>
#include <signal.h>

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

GMainLoop *mainloop;
xmmsc_connection_t *connection;
int vis;

short data[2];
char buf[38+38];

void draw () {
	int i, w;

	w = (int)(((double)data[0] / (double)SHRT_MAX) * 36.0);
	for (i = 0; i < 36 - w; ++i) {
		switch (buf[i]) {
			case '#':
				buf[i] = '*';
				break;
			case '*':
				buf[i] = '+';
				break;
			case '+':
				buf[i] = '-';
				break;
			default:
				buf[i] = ' ';
		}
	}
	for (i = 36 - w; i < 36; ++i)
		buf[i] = '#';

	buf[36] = buf[37] = ' ';

	w = (int)(((double)data[1] / (double)SHRT_MAX) * 36.0) + 38;
	for (i = 38; i < w; ++i)
		buf[i] = '#';
	for (i = w; i < 38+38; ++i) {
		switch (buf[i]) {
			case '#':
				buf[i] = '*';
				break;
			case '*':
				buf[i] = '+';
				break;
			case '+':
				buf[i] = '-';
				break;
			default:
				buf[i] = ' ';
		}
	}
	fputs (buf, stdout);
	putchar ('\r');
	fflush (stdout);
}

gboolean draw_gtk (gpointer stuff)
{
	int ret = xmmsc_visualization_chunk_get (connection, vis, data, 0, 0);
	if (ret == 2) {
		draw ();
	}
	return (ret >= 0);
}

void shutdown_gtk (gpointer stuff)
{
	g_main_loop_quit (mainloop);
}

int
main (int argc, char **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *configval;
	xmmsv_t *val;
	const char *errmsg;
	gchar *path = getenv ("XMMS_PATH");
	connection = xmmsc_init ("xmms2-vistest");

	if (!connection || !xmmsc_connect (connection, path)){
		printf ("Couldn't connect to xmms2d: %s\n",
		        xmmsc_get_last_error (connection));
		exit (EXIT_FAILURE);
	}

	res = xmmsc_visualization_version (connection);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		puts (errmsg);
		exit (EXIT_FAILURE);
	} else {
		int32_t version;
		xmmsv_get_int (val, &version);
		/* insert the version you need here or instead of complaining,
		   reduce your feature set to fit the version */
		if (version < 1) {
			printf ("The server only supports formats up to version %d (needed is %d)!", version, 1);
			exit (EXIT_FAILURE);
		}
	}
	xmmsc_result_unref (res);

	res = xmmsc_visualization_init (connection);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		puts (errmsg);
		exit (EXIT_FAILURE);
	}
	vis = xmmsc_visualization_init_handle (res);

	configval = xmmsv_build_dict (XMMSV_DICT_ENTRY_STR ("type", "peak"),
	                              XMMSV_DICT_ENTRY_STR ("stereo", "1"),
	                              XMMSV_DICT_END);

	res = xmmsc_visualization_properties_set (connection, vis, configval);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		puts (errmsg);
		exit (EXIT_FAILURE);
	}
	xmmsc_result_unref (res);
	xmmsv_unref (configval);

	while (!xmmsc_visualization_started (connection, vis)) {
		res = xmmsc_visualization_start (connection, vis);
		if (xmmsc_visualization_errored (connection, vis)) {
			printf ("Couldn't start visualization transfer: %s\n",
				xmmsc_get_last_error (connection));
			exit (EXIT_FAILURE);
		}
		if (res) {
			xmmsc_result_wait (res);
			xmmsc_visualization_start_handle (connection, res);
			xmmsc_result_unref (res);
		}
	}

	/* using GTK mainloop */
	mainloop = g_main_loop_new (NULL, FALSE);
	xmmsc_mainloop_gmain_init (connection);
	g_timeout_add_full (G_PRIORITY_DEFAULT, 20, draw_gtk, NULL, shutdown_gtk);
	g_main_loop_run (mainloop);

	/* not using GTK mainloop */
	while (xmmsc_visualization_chunk_get (connection, vis, data, 0, 1000) != -1) {
		draw ();
	}

	putchar ('\n');
	xmmsc_visualization_shutdown (connection, vis);

	if (connection) {
		xmmsc_unref (connection);
	}

	return 0;
}
