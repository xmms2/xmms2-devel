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
#include "xmms/visualisation.h"
#include "xmms/signal_xmms.h"
#include "xmms/ipc.h"


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
	xmms_output_t *output;
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
			xmms_log_error ("XMMS was unable to parse configfile %s", filename);
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
xmms_main_destroy (xmms_object_t *object)
{
	xmms_main_t *mainobj = (xmms_main_t *) object;
	xmms_object_cmd_arg_t arg;
	gchar filename[XMMS_MAX_CONFIGFILE_LEN];

	/* stop output */
	xmms_object_cmd_arg_init (&arg);

	xmms_object_cmd_call (XMMS_OBJECT (mainobj->output),
	                      XMMS_IPC_CMD_STOP, &arg);

	sleep(1); /* wait for the output thread to end */
	xmms_object_unref (mainobj->output);

	g_snprintf (filename, XMMS_MAX_CONFIGFILE_LEN, "%s/.xmms2/xmms2.conf", g_get_home_dir ());
	xmms_config_save (filename);

	xmms_visualisation_shutdown ();
	xmms_config_shutdown ();
	xmms_medialib_shutdown ();
	xmms_plugin_shutdown ();
	xmms_log_shutdown ();
}

static guint
hello (xmms_object_t *object, guint protocolver, gchar *client, xmms_error_t *error)
{
	XMMS_DBG ("Client %s with protocol version %d sent hello!", client, protocolver);
	return 1;
}

static void
quit (xmms_object_t *object, xmms_error_t *error)
{
	xmms_ipc_shutdown ();
	xmms_object_unref (object);

	exit (EXIT_SUCCESS);
}

XMMS_CMD_DEFINE (quit, quit, xmms_object_t*, NONE, NONE, NONE); 
XMMS_CMD_DEFINE (hello, hello, xmms_object_t *, UINT32, UINT32, STRING);


static void
on_output_volume_changed (xmms_object_t *object, gconstpointer data,
                          gpointer userdata)
{
	xmms_config_value_t *cfg;

	cfg = xmms_config_lookup (userdata);
	xmms_config_value_data_set (cfg, (gchar *) data);
}

static void
init_volume_config_proxy (const gchar *output)
{
	xmms_config_value_t *cfg;
	static gchar source[64];
	const gchar *vol;

	/* read the real volume value */
	g_snprintf (source, sizeof (source), "output.%s.volume", output);

	cfg = xmms_config_lookup (source);
	vol = xmms_config_value_string_get (cfg);

	xmms_config_value_callback_set (cfg, on_output_volume_changed,
	                                "output.volume");

	/* create the proxy value and assign the value */
	cfg = xmms_config_value_register ("output.volume", vol,
	                                  on_output_volume_changed,
	                                  source);
	xmms_config_value_data_set (cfg, (gchar *) vol);
}

static void usage (void)
{
	static char *usageText = "XMMS2 Daemon\n\
Options:\n\
	-v		Increase verbosity\n\
	-V|--version	Print version\n\
	-n		Disable logging\n\
	-o <x>		Use 'x' as output plugin\n\
	-d		Daemonise\n\
	-p <foo>	Search for plugins in directory 'foo'\n\
	-h|--help	Print this help\n";
       printf(usageText);
}

int
main (int argc, char **argv)
{
	xmms_plugin_t *o_plugin;
	xmms_config_value_t *cv;
	xmms_main_t *mainobj;
	xmms_ipc_t *ipc;

	int opt;
	int verbose = 0;
	sigset_t signals;
	xmms_playlist_t *playlist;
	const gchar *outname = NULL;
	gboolean daemonize = FALSE;
	gboolean doLog = TRUE;
	gchar default_path[XMMS_PATH_MAX + 16];
	gchar *ppath = NULL;
	pid_t ppid=0;

	memset (&signals, 0, sizeof (sigset_t));
	sigaddset (&signals, SIGHUP);
	sigaddset (&signals, SIGTERM);
	sigaddset (&signals, SIGINT);
	sigaddset (&signals, SIGPIPE);
	pthread_sigmask (SIG_BLOCK, &signals, NULL);

	static struct option long_opts[] = {
		{"version", 0, NULL, 'V'},
		{"help", 0, NULL, 'h'}
	};

	while (42) {
		opt = getopt_long (argc, argv, "dvVno:p:h", long_opts, NULL);

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
			case 'h':
				usage();
				exit(0);
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

	g_random_set_seed (time (NULL));

	ipc = xmms_ipc_init ();

	parse_config ();
	
	xmms_log_init (doLog ? "xmmsd" : "null");
	
	playlist = xmms_playlist_init ();

	xmms_visualisation_init ();
	
	if (!xmms_plugin_init (ppath))
		return 1;

	
	if (!outname) {
		cv = xmms_config_value_register ("output.plugin",
		                                 XMMS_OUTPUT_DEFAULT,
		                                 change_output, NULL);
		outname = xmms_config_value_string_get (cv);
	}

	XMMS_DBG ("output = %s", outname);

	xmms_config_value_register ("decoder.buffersize", 
			XMMS_DECODER_DEFAULT_BUFFERSIZE, NULL, NULL);
	xmms_config_value_register ("transport.buffersize", 
			XMMS_TRANSPORT_DEFAULT_BUFFERSIZE, NULL, NULL);
	xmms_config_value_register ("decoder.use_replaygain", "0",
	                            NULL, NULL);
	xmms_config_value_register ("decoder.replaygain_mode", "track",
	                            NULL, NULL);
	xmms_config_value_register ("decoder.use_replaygain_anticlip", "1",
	                            NULL, NULL);

	o_plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_OUTPUT, outname);
	g_return_val_if_fail (o_plugin, -1);

	mainobj = xmms_object_new (xmms_main_t, xmms_main_destroy);

	mainobj->output = xmms_output_new (o_plugin);
	g_return_val_if_fail (mainobj->output, -1);

	init_volume_config_proxy (outname);

	xmms_output_playlist_set (mainobj->output, playlist);

	xmms_medialib_init ();
	xmms_medialib_output_register (mainobj->output);
	xmms_medialib_playlist_set (playlist);
		
	g_snprintf (default_path, sizeof (default_path),
	            "unix:///tmp/xmms-ipc-%s", g_get_user_name ());
	cv = xmms_config_value_register ("core.ipcsocket", default_path,
	                                 NULL, NULL);

	if (!xmms_ipc_setup_server (xmms_config_value_string_get (cv))) {
		xmms_log_fatal ("IPC failed to init!");
	}

	xmms_ipc_setup_with_gmain (ipc);

	xmms_signal_init (XMMS_OBJECT (mainobj));

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MAIN, XMMS_OBJECT (mainobj));
	xmms_object_cmd_add (XMMS_OBJECT (mainobj), XMMS_IPC_CMD_QUIT, XMMS_CMD_FUNC (quit));
	xmms_object_cmd_add (XMMS_OBJECT (mainobj), XMMS_IPC_CMD_HELLO, XMMS_CMD_FUNC (hello));

	if (ppid) { /* signal that we are inited */
		kill (ppid, SIGUSR1);
	}

	mainloop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (mainloop);

	return 0;
}

/** @} */
