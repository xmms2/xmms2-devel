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




/** @file 
 * This file controls XMMS2 mainloop.
 */

/** @defgroup XMMSServer XMMSServer
  * @brief look at this if you want to code inside the server.
  * The XMMS2 project is splitted in to a server part and a Clientpart.
  * This documents the server part of the project.
  */

/**
  * @defgroup Main
  * @ingroup XMMSServer
  * @{ 
  */

#include <glib.h>

#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/config.h"
#include "xmms/playlist.h"
#include "xmms/unixsignal.h"
#include "xmms/util.h"
#include "xmms/medialib.h"
#include "xmms/output.h"
#include "xmms/xmms.h"
#include "xmms/effect.h"
#include "xmms/dbus.h"
#include "xmms/visualisation.h"
#include "xmms/signal_xmms.h"


#include "internal/plugin_int.h"
#include "internal/output_int.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <pthread.h>

struct xmms_main_St {
	xmms_object_t object;
};

typedef struct xmms_main_St xmms_main_t;

static GMainLoop *mainloop;

static gboolean
parse_config ()
{
	gchar filename[XMMS_MAX_CONFIGFILE_LEN];
	gchar configdir[XMMS_MAX_CONFIGFILE_LEN];

	g_snprintf (filename, XMMS_MAX_CONFIGFILE_LEN, "%s/.xmms2/xmms2.conf", g_get_home_dir ());
	g_snprintf (configdir, XMMS_MAX_CONFIGFILE_LEN, "%s/.xmms2/", g_get_home_dir ());

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		if (!xmms_config_init (filename)) {
			XMMS_DBG ("XMMS was unable to parse configfile %s", filename);
			exit (EXIT_FAILURE);
		}
		return TRUE;
	} else {
		if (!g_file_test (configdir, G_FILE_TEST_IS_DIR)) {
			mkdir (configdir, 0755);
		}

		xmms_config_init (NULL);

		return TRUE;
	}
	return FALSE;
}


static void
change_output (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	gchar *outname = (gchar *)data;

	XMMS_DBG ("Want to use %s as output instead", outname);

	/** @todo fix this */
}

static void
quit (xmms_object_t *object, xmms_error_t *error) 
{
	exit (EXIT_SUCCESS);
}

XMMS_METHOD_DEFINE (quit, quit, xmms_object_t*, NONE, NONE, NONE); 

int
main (int argc, char **argv)
{
	xmms_plugin_t *o_plugin;
	xmms_config_value_t *cv;
	xmms_output_t *output;
	xmms_main_t *mainobj;

	int opt;
	int verbose = 0;
	sigset_t signals;
	xmms_playlist_t *playlist;
	const gchar *outname = NULL;
	gboolean daemonize = FALSE;
	gboolean doLog = TRUE;
	const gchar *path;
	gchar *ppath = NULL;
	pid_t ppid=0;

	memset (&signals, 0, sizeof (sigset_t));
	sigaddset (&signals, SIGHUP);
	sigaddset (&signals, SIGTERM);
	sigaddset (&signals, SIGINT);
	pthread_sigmask (SIG_BLOCK, &signals, NULL);

	
	while (42) {
		opt = getopt (argc, argv, "dvVno:p:");

		if (opt == -1)
			break;

		switch (opt) {
			case 'v':
				verbose++;
				break;

			case 'V':
				printf ("XMMS version %s\n", XMMS_VERSION);
				exit (0);
				break;

			case 'n':
				doLog = FALSE;
				break;

			case 'o':
				outname = g_strdup (optarg);
				break;

			case 'd':
				daemonize = TRUE;
				break;
			case 'p':
				ppath = g_strdup (optarg);
				break;
				
		}
	}

	if (daemonize) {
		ppid = getpid ();
		if (fork ()) {
			sigset_t signals;
			int caught;
			memset (&signals, 0, sizeof (sigset_t));
			sigaddset (&signals, SIGUSR1);
			sigaddset (&signals, SIGCHLD);
			sigwait (&signals, &caught);
			exit (caught != SIGUSR1);
		}
		setsid();
		if (fork ()) exit(0);
		xmms_log_daemonize ();
	}

	g_thread_init (NULL);

	xmms_log_initialize (doLog?"xmmsd":"null");
	
	parse_config ();
	
	playlist = xmms_playlist_init ();

	xmms_visualisation_init_mutex ();
	
	if (!xmms_plugin_init (ppath))
		return 1;

	
	if (!outname) {
		cv = xmms_config_value_register ("core.outputplugin",
		                                 XMMS_OUTPUT_DEFAULT,
		                                 change_output, NULL);
		outname = xmms_config_value_string_get (cv);
	}

	XMMS_DBG ("output = %s", outname);

	xmms_config_value_register ("core.decoder_buffersize", 
			XMMS_DECODER_DEFAULT_BUFFERSIZE, NULL, NULL);
	xmms_config_value_register ("core.transport_buffersize", 
			XMMS_TRANSPORT_DEFAULT_BUFFERSIZE, NULL, NULL);

	o_plugin = xmms_output_find_plugin (outname);
	g_return_val_if_fail (o_plugin, -1);

	output = xmms_output_new (o_plugin);
	g_return_val_if_fail (output, -1);

	xmms_output_playlist_set (output, playlist);
		
	path = g_strdup_printf ("unix:path=/tmp/xmms-dbus-%s", 
				g_get_user_name ());
	cv = xmms_config_value_register ("core.dbuspath", path, NULL, NULL);
	path = xmms_config_value_string_get (cv);

	xmms_dbus_init (path);

	xmms_signal_init ();

	mainobj = xmms_object_new (xmms_main_t, NULL);
	xmms_dbus_register_object ("main", XMMS_OBJECT (mainobj));
	xmms_object_method_add (XMMS_OBJECT (mainobj), XMMS_METHOD_QUIT, XMMS_METHOD_FUNC (quit));

	if (ppid) { /* signal that we are inited */
		kill (ppid, SIGUSR1);
	}

	mainloop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (mainloop);

	return 0;
}

/** @} */
