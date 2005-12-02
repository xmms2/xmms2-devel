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
 * This file controls the XMMS2 main loop.
 */

#include <glib.h>

#include "xmmspriv/xmms_plugin.h"
#include "xmmspriv/xmms_transport.h"
#include "xmmspriv/xmms_decoder.h"
#include "xmmspriv/xmms_config.h"
#include "xmmspriv/xmms_playlist.h"
#include "xmmspriv/xmms_unixsignal.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_output.h"
#include "xmmspriv/xmms_effect.h"
#include "xmmspriv/xmms_visualisation.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_log.h"
#include "xmms/xmms_defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <pthread.h>

#ifdef XMMS_OS_LINUX 
# define XMMS_OUTPUT_DEFAULT "alsa"
#elif XMMS_OS_OPENBSD
# define XMMS_OUTPUT_DEFAULT "oss"
#elif XMMS_OS_NETBSD
# define XMMS_OUTPUT_DEFAULT "oss"
#elif XMMS_OS_SOLARIS
# define XMMS_OUTPUT_DEFAULT "sun"
#elif XMMS_OS_DARWIN
# define XMMS_OUTPUT_DEFAULT "coreaudio"
#elif XMMS_OS_FREEBSD
# define XMMS_OUTPUT_DEFAULT "oss"
#endif

static void quit (xmms_object_t *object, xmms_error_t *error);
static GHashTable *status (xmms_object_t *object, xmms_error_t *error);
static guint hello (xmms_object_t *object, guint protocolver, gchar *client, xmms_error_t *error);
static void install_scripts (const gchar *into_dir);

XMMS_CMD_DEFINE (quit, quit, xmms_object_t*, NONE, NONE, NONE); 
XMMS_CMD_DEFINE (hello, hello, xmms_object_t *, UINT32, UINT32, STRING);
XMMS_CMD_DEFINE (status, status, xmms_object_t *, DICT, NONE, NONE);
XMMS_CMD_DEFINE (plugin_list, xmms_plugin_client_list, xmms_object_t *, LIST, UINT32, NONE);

/** @defgroup XMMSServer XMMSServer
  * @brief look at this if you want to code inside the server.
  * The XMMS2 project is split into a server and a multiple clients.
  * This documents the server part.
  */

/**
  * @defgroup Main Main
  * @ingroup XMMSServer
  * @brief main object
  * @{ 
  */


/**
 * Main object, when this is unreffed, XMMS2 is quiting.
 */
struct xmms_main_St {
	xmms_object_t object;
	xmms_output_t *output;
	time_t starttime;
};

typedef struct xmms_main_St xmms_main_t;

static GMainLoop *mainloop;
static gchar *conffile = NULL;

static GHashTable *
status (xmms_object_t *object, xmms_error_t *error)
{
	gint starttime;
	GHashTable *ret = g_hash_table_new_full (g_str_hash, g_str_equal,
											 g_free, xmms_object_cmd_value_free);

	starttime = ((xmms_main_t*)object)->starttime;

	g_hash_table_insert (ret, g_strdup ("version"),
						 xmms_object_cmd_value_str_new (XMMS_VERSION));
	g_hash_table_insert (ret, g_strdup ("uptime"),
						 xmms_object_cmd_value_int_new (time(NULL)-starttime));

	return ret;
}

/**
 * @internal Execute all programs or scripts in a directory. Used when starting
 * up and shutting down the daemon.
 *
 * @param[in] scriptdir Directory to search for executable programs/scripts.
 * started.
 */
static void
do_scriptdir (const gchar *scriptdir)
{
	GError *err;
	GDir *dir;
	const gchar *f;
	gchar *argv[2] = {NULL, NULL};

	XMMS_DBG ("Running scripts in %s", scriptdir);
	if (!g_file_test (scriptdir, G_FILE_TEST_IS_DIR)) {
		mkdir (scriptdir, 0755);
		install_scripts (scriptdir);
	}

	dir = g_dir_open (scriptdir, 0, &err);
	if (!dir) {
		XMMS_DBG ("Could not open %s error: %s", scriptdir, err->message);
		return;
	}

	while ((f = g_dir_read_name (dir))) {
		argv[0] = g_strdup_printf ("%s/%s", scriptdir, f);
		if (g_file_test (argv[0], G_FILE_TEST_IS_EXECUTABLE)) {
			g_spawn_async (g_get_home_dir(), argv, NULL,
				       0,
				       NULL, NULL, NULL, &err);
		}
		g_free (argv[0]);
	}

	g_dir_close (dir);

}

/**
 * @internal Load the xmms2d configuration file. Creates the config directory
 * if needed.
 */
static void
load_config ()
{
	gchar configdir[XMMS_MAX_CONFIGFILE_LEN];

	if (!conffile) {
		conffile = g_strdup_printf ("%s/.xmms2/xmms2.conf", g_get_home_dir ());
	}

	g_assert (strlen (conffile) <= XMMS_MAX_CONFIGFILE_LEN);

	g_snprintf (configdir, XMMS_MAX_CONFIGFILE_LEN, "%s/.xmms2/", g_get_home_dir ());
	if (!g_file_test (configdir, G_FILE_TEST_IS_DIR)) {
		mkdir (configdir, 0755);
	}

	xmms_config_init(conffile);
}

/**
 * @internal Switch to using another output plugin
 * @param object An object
 * @param data The name of the output plugin to switch to
 * @param userdata The #xmms_main_t object
 */
static void
change_output (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	xmms_plugin_t *plugin;
	xmms_main_t *mainobj = (xmms_main_t*)userdata;
	gchar *outname = (gchar *) data;

	if (!mainobj->output)
		return;

	XMMS_DBG ("Want to use %s as output instead", outname);

	plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_OUTPUT, outname);
	if (!plugin) {
		xmms_log_error ("Baaaaad output plugin, try to change the output.plugin config variable to something usefull");
	} else {
		if (!xmms_output_plugin_switch (mainobj->output, plugin)) {
			xmms_log_error ("Baaaaad output plugin, try to change the output.plugin config variable to something usefull");
		}
	}
}

/**
 * @internal Destroy the main object
 * @param[in] object The object to destroy
 */
static void
xmms_main_destroy (xmms_object_t *object)
{
	xmms_main_t *mainobj = (xmms_main_t *) object;
	xmms_object_cmd_arg_t arg;
	xmms_config_property_t *cv;

	cv = xmms_config_lookup ("core.shutdownpath");
	do_scriptdir (xmms_config_property_get_string (cv));
	
	/* stop output */
	xmms_object_cmd_arg_init (&arg);

	xmms_object_cmd_call (XMMS_OBJECT (mainobj->output),
	                      XMMS_IPC_CMD_STOP, &arg);

	sleep(1); /* wait for the output thread to end */
	xmms_object_unref (mainobj->output);

	g_assert (conffile != NULL);
	xmms_config_save (conffile);

	xmms_visualisation_shutdown ();
	xmms_config_shutdown ();
	xmms_plugin_shutdown ();

	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_MAIN);
	xmms_ipc_shutdown ();

	xmms_log_shutdown ();
}

/**
 * @internal Function to respond to the 'hello' sent from clients on connect
 */
static guint
hello (xmms_object_t *object, guint protocolver, gchar *client, xmms_error_t *error)
{
	XMMS_DBG ("Client %s with protocol version %d sent hello!", client, protocolver);
	return 1;
}

/**
 * @internal Function to respond to the 'quit' command sent from a client
 */
static void
quit (xmms_object_t *object, xmms_error_t *error)
{
	xmms_object_unref (object);

	exit (EXIT_SUCCESS);
}


/**
 * @internal Callback function executed whenever the output volume is changed.
 * Simply sets the configuration value as needed.
 */
static void
on_output_volume_changed (xmms_object_t *object, gconstpointer data,
                          gpointer userdata)
{
	xmms_config_property_t *cfg;

	cfg = xmms_config_lookup (userdata);
	xmms_config_property_set_data (cfg, (gchar *) data);
}

/**
 * @internal Initialise volume proxy setting. Using a proxy configuration value
 * to modify volume level means that the client does not need to know which
 * output plugin the daemon is currently using - it simply modifies the proxy
 * value and the daemon takes care of the rest.
 * @param[in] output The name of the current output plugin.
 */
static void
init_volume_config_proxy (const gchar *output)
{
	xmms_config_property_t *cfg;
	static gchar source[64];
	const gchar *vol;

	/* read the real volume value */
	g_snprintf (source, sizeof (source), "output.%s.volume", output);

	cfg = xmms_config_lookup (source);
	if (cfg) {
		vol = xmms_config_property_get_string (cfg);

		xmms_config_property_callback_set (cfg, on_output_volume_changed,
						  				"output.volume");

		/* create the proxy value and assign the value */
		cfg = xmms_config_property_register ("output.volume", vol,
										on_output_volume_changed,
									  	source);
		xmms_config_property_set_data (cfg, (gchar *) vol);
	}
}

static gboolean
symlink_file (gchar *source, gchar *dest)
{
	gint r;

	g_return_val_if_fail (source, FALSE);
	g_return_val_if_fail (dest, FALSE);

	r = symlink (source, dest);

	return r != -1;
}

static void
install_scripts (const gchar *into_dir)
{
	GDir *dir;
	GError *err;
	gchar path[PATH_MAX];
	const gchar *f;
	gchar *s;

	s = strrchr (into_dir, '/');
	if (!s)
		return;

	s++;

	g_snprintf (path, PATH_MAX, "%s/scripts/%s", SHAREDDIR, s);
	XMMS_DBG ("installing scripts into %s", path);
	dir = g_dir_open (path, 0, &err);
	if (!dir) {
		XMMS_DBG ("global script directory not found");
		return;
	}

	while ((f = g_dir_read_name (dir))) {
		gchar *source = g_strdup_printf ("%s/%s", path, f);
		gchar *dest = g_strdup_printf ("%s/%s", into_dir, f);
		if (!symlink_file (source, dest)) {
			break;
		}
		g_free (source);
		g_free (dest);
	}

	g_dir_close (dir);
}

/**
 * @internal Print a simple message detailing command line options for xmms2d
 */
static void usage (void)
{
	static char *usageText = "XMMS2 Daemon\n\
Options:\n\
	-v		Increase verbosity\n\
	-q		Decrease verbosity\n\
	-V|--version	Print version\n\
	-n		Disable logging\n\
	-o <x>		Use 'x' as output plugin\n\
	-i <url>	Listen to socket 'url'\n\
	-p <foo>	Search for plugins in directory 'foo'\n\
	-h|--help	Print this help\n\
	-c|--conf=<file> Specify alternate configuration file\n\
	-s|--status-fd=fd Specify a filedescriptor to write to when started\n";
       printf(usageText);
}

/* @endif */

/**
 * The xmms2 daemon main initialisation function
 */
int
main (int argc, char **argv)
{
	xmms_plugin_t *o_plugin;
	xmms_config_property_t *cv;
	xmms_main_t *mainobj;
	xmms_ipc_t *ipc;
	int status_fd = -1;
	int opt;
	int verbose = 1;
	sigset_t signals;
	xmms_playlist_t *playlist;
	const gchar *outname = NULL;
	gchar default_path[XMMS_PATH_MAX + 16];
	gchar *ppath = NULL;
	gchar *tmp;
	const gchar *ipcpath = NULL;
	static struct option long_opts[] = {
		{"version", 0, NULL, 'V'},
		{"help", 0, NULL, 'h'},
		{"conf", 1, NULL, 'c'},
		{"status-fd", 1, NULL, 's'},
		{NULL,}
	};

	memset (&signals, 0, sizeof (sigset_t));
	sigaddset (&signals, SIGHUP);
	sigaddset (&signals, SIGTERM);
	sigaddset (&signals, SIGINT);
	sigaddset (&signals, SIGPIPE);
	pthread_sigmask (SIG_BLOCK, &signals, NULL);

	while (42) {
		opt = getopt_long (argc, argv, "vqVno:i:p:hc:s:", long_opts, NULL);

		if (opt == -1)
			break;

		switch (opt) {
			case 'v':
				verbose++;
				break;
			case 'q':
				verbose--;
				break;
			case 'V':
				printf ("XMMS version %s\n", XMMS_VERSION);
				exit (0);
				break;
			case 'o':
				outname = g_strdup (optarg);
				break;
			case 'p':
				ppath = g_strdup (optarg);
				break;
			case 'h':
				usage();
				exit(0);
				break;
			case 'c':
				conffile = g_strdup (optarg);
				break;
			case 'i':
				ipcpath = g_strdup (optarg);
				break;
			case 's':
				status_fd = atoi (optarg);
				break;

		}
	}

	g_thread_init (NULL);

	g_random_set_seed (time (NULL));

	xmms_log_init (verbose);

	ipc = xmms_ipc_init ();
	
	load_config ();

	xmms_config_property_register ("decoder.buffersize", 
			XMMS_DECODER_DEFAULT_BUFFERSIZE, NULL, NULL);
	xmms_config_property_register ("transport.buffersize", 
			XMMS_TRANSPORT_DEFAULT_BUFFERSIZE, NULL, NULL);


	if (!xmms_plugin_init (ppath))
		return 1;

	playlist = xmms_playlist_init ();

	xmms_visualisation_init ();
	
	mainobj = xmms_object_new (xmms_main_t, xmms_main_destroy);

	/* find output plugin. */
	cv = xmms_config_property_register ("output.plugin",
	                                 XMMS_OUTPUT_DEFAULT,
	                                 change_output, mainobj);

	if (outname)
		xmms_config_setvalue (NULL, "output.plugin", outname, NULL);

	outname = xmms_config_property_get_string (cv);

	XMMS_DBG ("output = %s", outname);

	o_plugin = xmms_plugin_find (XMMS_PLUGIN_TYPE_OUTPUT, outname);

	if (!o_plugin) {
		xmms_log_error ("Baaaaad output plugin, try to change the output.plugin config variable to something usefull");
	}

	mainobj->output = xmms_output_new (o_plugin, playlist);
	if (!mainobj->output) {
		xmms_log_fatal ("Failed to create output object!");
	}
	init_volume_config_proxy (outname);

	g_snprintf (default_path, sizeof (default_path),
	            "unix:///tmp/xmms-ipc-%s", g_get_user_name ());
	cv = xmms_config_property_register ("core.ipcsocket", default_path,
	                                 NULL, NULL);

	if (!ipcpath)
		ipcpath = xmms_config_property_get_string (cv);
	if (!xmms_ipc_setup_server (ipcpath)) {
		xmms_log_fatal ("IPC failed to init!");
	}

	if (status_fd != -1) {
		write (status_fd, "+", 1);
	}

	xmms_ipc_setup_with_gmain (ipc);

	xmms_signal_init (XMMS_OBJECT (mainobj));

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MAIN, XMMS_OBJECT (mainobj));
	xmms_object_cmd_add (XMMS_OBJECT (mainobj), XMMS_IPC_CMD_QUIT, XMMS_CMD_FUNC (quit));
	xmms_object_cmd_add (XMMS_OBJECT (mainobj), XMMS_IPC_CMD_HELLO, XMMS_CMD_FUNC (hello));
	xmms_object_cmd_add (XMMS_OBJECT (mainobj), XMMS_IPC_CMD_PLUGIN_LIST, XMMS_CMD_FUNC (plugin_list));
	xmms_object_cmd_add (XMMS_OBJECT (mainobj), XMMS_IPC_CMD_STATUS, XMMS_CMD_FUNC (status));
	mainobj->starttime = time (NULL);


	putenv (g_strdup_printf ("XMMS_PATH=%s", ipcpath));

	tmp = g_strdup_printf ("%s/.xmms2/shutdown.d", g_get_home_dir());
	cv = xmms_config_property_register ("core.shutdownpath",
				    tmp, NULL, NULL);
	g_free (tmp);

	tmp = g_strdup_printf ("%s/.xmms2/startup.d", g_get_home_dir());
	cv = xmms_config_property_register ("core.startuppath",
				    tmp, NULL, NULL);
	g_free (tmp);

	/* Startup dir */
	do_scriptdir (xmms_config_property_get_string (cv));

	mainloop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (mainloop);

	return 0;
}

/** @} */
