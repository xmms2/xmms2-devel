/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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
#include <stdio.h>
#include <signal.h>

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

static GMainLoop *mainloop;
static xmmsc_connection_t *connection;
static int vis;
static int height;

static short data[256];

static void
draw (void) {
	int i, z, val;
	int base = SHRT_MAX / height;

	printf ("\e[1m");
	for (z = height; z >= 1; z -= 2) {
		for (i = 0; i < 90; i += 1) {
			/*val = (data[i] + data[i+1]) / 2;*/
			val = data[i];
			if (val > (z * base)) {
				putchar ('|');
			} else if (val > ((z-1) * base)) {
				putchar ('.');
			} else {
				putchar (' ');
			}
		}
		putchar ('\n');
	}
	printf ("\e[m\e[%dA", height/2);
	fflush (stdout);
}

static gboolean
draw_gtk (gpointer stuff)
{
	int ret = xmmsc_visualization_chunk_get (connection, vis, data, 0, 0);
	if (ret == 256) {
		draw ();
	}
	return (ret >= 0);
}

static void
shutdown_gtk (gpointer stuff)
{
	g_main_loop_quit (mainloop);
}

static void
quit (int signum)
{
	printf ("\e[%dB", height/2);

	xmmsc_visualization_shutdown (connection, vis);
	if (connection) {
		xmmsc_unref (connection);
	}

	exit (EXIT_SUCCESS);
}

static void
setup_signal (void)
{
	struct sigaction siga;
	siga.sa_handler = quit;
	sigemptyset (&siga.sa_mask);
	siga.sa_flags = SA_RESTART;
	sigaction(SIGINT, &siga, (struct sigaction*)NULL);
}

int
main (int argc, char **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	xmmsv_t *configval;
	gchar *path = getenv ("XMMS_PATH");
	connection = xmmsc_init ("xmms2-vistest");

	if (argc != 2 || (height = atoi (argv[1]) * 2) < 1) {
		printf ("Usage: %s <lines>\n", argv[0]);
		exit (EXIT_SUCCESS);
	}

	if (!connection || !xmmsc_connect (connection, path)) {
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

	configval = xmmsv_build_dict (XMMSV_DICT_ENTRY_STR ("type", "spectrum"),
	                              XMMSV_DICT_ENTRY_STR ("stereo", "0"),
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

	setup_signal ();

	/* using GTK mainloop */
	mainloop = g_main_loop_new (NULL, FALSE);
	xmmsc_mainloop_gmain_init (connection);
	g_timeout_add_full (G_PRIORITY_DEFAULT, 20, draw_gtk, NULL, shutdown_gtk);
	g_main_loop_run (mainloop);

	/* not using GTK mainloop */
	while (xmmsc_visualization_chunk_get (connection, vis, data, 0, 1000) == 256) {
		draw ();
	}

	quit (0);
	return 0;
}
