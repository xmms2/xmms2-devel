/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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

/**
 * @mainpage
 * @image html pixmaps/xmms2-128.png
 */

/** @file
 * This file controls the XMMS2 main loop.
 */

#include <locale.h>
#include <glib.h>

#include "xmms_configuration.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmmspriv/xmms_config.h"
#include "xmmspriv/xmms_playlist.h"
#include "xmmspriv/xmms_playlist_updater.h"
#include "xmmspriv/xmms_collsync.h"
#include "xmmspriv/xmms_collection.h"
#include "xmmspriv/xmms_signal.h"
#include "xmmspriv/xmms_symlink.h"
#include "xmmspriv/xmms_checkroot.h"
#include "xmmspriv/xmms_thread_name.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_mediainfo.h"
#include "xmmspriv/xmms_output.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_log.h"
#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_bindata.h"
#include "xmmspriv/xmms_utils.h"
#include "xmmspriv/xmms_visualization.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * Forward declarations of the methods in the main object
 */
static void xmms_main_client_quit (xmms_object_t *object, xmms_error_t *error);
static GTree *xmms_main_client_stats (xmms_object_t *object, xmms_error_t *error);
static GList *xmms_main_client_list_plugins (xmms_object_t *main, gint32 type, xmms_error_t *err);
static void xmms_main_client_hello (xmms_object_t *object, gint protocolver, const gchar *client, xmms_error_t *error);
static void install_scripts (const gchar *into_dir);
static void spawn_script_setup (gpointer data);

#include "main_ipc.c"

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
	xmms_output_t *output_object;
	xmms_bindata_t *bindata_object;
	xmms_coll_dag_t *colldag_object;
	xmms_medialib_t *medialib_object;
	xmms_playlist_t *playlist_object;
	xmms_coll_sync_t *collsync_object;
	xmms_playlist_updater_t *plsupdater_object;
	xmms_xform_object_t *xform_object;
	xmms_mediainfo_reader_t *mediainfo_object;
	xmms_visualization_t *visualization_object;
	time_t starttime;
};

typedef struct xmms_main_St xmms_main_t;

/** This is the mainloop of the xmms2 server */
static GMainLoop *mainloop;

/** The path of the configfile */
static gchar *conffile = NULL;

/**
 * This returns the main stats for the server
 */
static GTree *
xmms_main_client_stats (xmms_object_t *object, xmms_error_t *error)
{
	GTree *ret;
	gint starttime;

	ret = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
	                       NULL, (GDestroyNotify) xmmsv_unref);

	starttime = ((xmms_main_t*)object)->starttime;

	g_tree_insert (ret, (gpointer) "version",
	               xmmsv_new_string (XMMS_VERSION));
	g_tree_insert (ret, (gpointer) "uptime",
	               xmmsv_new_int (time (NULL) - starttime));

	return ret;
}

static gboolean
xmms_main_client_list_foreach (xmms_plugin_t *plugin, gpointer data)
{
	xmmsv_t *dict;
	GList **list = data;

	dict = xmmsv_build_dict (
	        XMMSV_DICT_ENTRY_STR ("name", xmms_plugin_name_get (plugin)),
	        XMMSV_DICT_ENTRY_STR ("shortname", xmms_plugin_shortname_get (plugin)),
	        XMMSV_DICT_ENTRY_STR ("version", xmms_plugin_version_get (plugin)),
	        XMMSV_DICT_ENTRY_STR ("description", xmms_plugin_description_get (plugin)),
	        XMMSV_DICT_ENTRY_INT ("type", xmms_plugin_type_get (plugin)),
	        XMMSV_DICT_END);

	*list = g_list_prepend (*list, dict);

	return TRUE;
}

static GList *
xmms_main_client_list_plugins (xmms_object_t *main, gint32 type, xmms_error_t *err)
{
	GList *list = NULL;
	xmms_plugin_foreach (type, xmms_main_client_list_foreach, &list);
	return list;
}


/**
 * @internal Execute all programs or scripts in a directory. Used when starting
 * up and shutting down the daemon.
 *
 * @param[in] scriptdir Directory to search for executable programs/scripts.
 * started.
 * @param     arg1 value passed to executed scripts as argument 1. This makes
 * it possible to handle start and stop in one script
 */
static void
do_scriptdir (const gchar *scriptdir, const gchar *arg1)
{
	GError *err = NULL;
	GDir *dir;
	const gchar *f;
	gchar *argv[3] = {NULL, NULL, NULL};

	XMMS_DBG ("Running scripts in %s", scriptdir);
	if (!g_file_test (scriptdir, G_FILE_TEST_IS_DIR)) {
		g_mkdir_with_parents (scriptdir, 0755);
		install_scripts (scriptdir);
	}

	dir = g_dir_open (scriptdir, 0, &err);
	if (!dir) {
		xmms_log_error ("Could not open script dir '%s' error: %s", scriptdir, err->message);
		return;
	}

	argv[1] = g_strdup (arg1);
	while ((f = g_dir_read_name (dir))) {
		argv[0] = g_strdup_printf ("%s/%s", scriptdir, f);
		if (g_file_test (argv[0], G_FILE_TEST_IS_EXECUTABLE)) {
			if (!g_spawn_async (g_get_home_dir (), argv, NULL, 0,
			                    spawn_script_setup, NULL, NULL, &err)) {
				xmms_log_error ("Could not run script '%s', error: %s",
				                argv[0], err->message);
			}
		}
		g_free (argv[0]);
	}
	g_free (argv[1]);

	g_dir_close (dir);

}

/**
 * @internal Setup function for processes spawned by do_scriptdir
 */
static void
spawn_script_setup (gpointer data)
{
	xmms_signal_restore ();
}

/**
 * @internal Load the xmms2d configuration file. Creates the config directory
 * if needed.
 */
static void
load_config (void)
{
	gchar configdir[XMMS_PATH_MAX];

	if (!conffile) {
		conffile = XMMS_BUILD_PATH ("xmms2.conf");
	}

	g_assert (strlen (conffile) <= XMMS_MAX_CONFIGFILE_LEN);

	if (!xmms_userconfdir_get (configdir, sizeof (configdir))) {
		xmms_log_error ("Could not get path to config dir");
	} else if (!g_file_test (configdir, G_FILE_TEST_IS_DIR)) {
		g_mkdir_with_parents (configdir, 0755);
	}

	xmms_config_init (conffile);
}

/**
 * @internal Switch to using another output plugin
 * @param object An object
 * @param data The name of the output plugin to switch to
 * @param userdata The #xmms_main_t object
 */
static void
change_output (xmms_object_t *object, xmmsv_t *_data, gpointer userdata)
{
	xmms_output_plugin_t *plugin;
	xmms_main_t *mainobj = (xmms_main_t*)userdata;
	const gchar *outname;

	if (!mainobj->output_object)
		return;

	outname = xmms_config_property_get_string ((xmms_config_property_t *) object);

	xmms_log_info ("Switching to output %s", outname);

	plugin = (xmms_output_plugin_t *)xmms_plugin_find (XMMS_PLUGIN_TYPE_OUTPUT, outname);
	if (!plugin) {
		xmms_log_error ("Baaaaad output plugin, try to change the output.plugin config variable to something useful");
	} else {
		if (!xmms_output_plugin_switch (mainobj->output_object, plugin)) {
			xmms_log_error ("Baaaaad output plugin, try to change the output.plugin config variable to something useful");
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
	do_scriptdir (xmms_config_property_get_string (cv), "stop");

	/* stop output */
	xmms_object_cmd_arg_init (&arg);
	arg.args = xmmsv_new_list ();
	xmms_object_cmd_call (XMMS_OBJECT (mainobj->output_object),
	                      XMMS_IPC_CMD_STOP, &arg);
	xmmsv_unref (arg.args);

	g_usleep (G_USEC_PER_SEC); /* wait for the output thread to end */

	xmms_object_unref (mainobj->output_object);
	xmms_object_unref (mainobj->bindata_object);
	xmms_object_unref (mainobj->medialib_object);
	xmms_object_unref (mainobj->playlist_object);
	xmms_object_unref (mainobj->xform_object);
	xmms_object_unref (mainobj->mediainfo_object);
	xmms_object_unref (mainobj->visualization_object);

	xmms_config_save ();

	xmms_config_shutdown ();

	xmms_plugin_shutdown ();

	xmms_main_unregister_ipc_commands ();

	xmms_ipc_shutdown ();

	xmms_log_shutdown ();
}

/**
 * @internal Function to respond to the 'hello' sent from clients on connect
 */
static void
xmms_main_client_hello (xmms_object_t *object, gint protocolver, const gchar *client, xmms_error_t *error)
{
	if (protocolver != XMMS_IPC_PROTOCOL_VERSION) {
		xmms_log_info ("Client '%s' with bad protocol version (%d, not %d) connected", client, protocolver, XMMS_IPC_PROTOCOL_VERSION);
		xmms_error_set (error, XMMS_ERROR_INVAL, "Bad protocol version");
		return;
	}
	XMMS_DBG ("Client '%s' connected", client);
}

static gboolean
kill_server (gpointer object) {
	xmms_object_emit_f (XMMS_OBJECT (object),
	                    XMMS_IPC_SIGNAL_QUIT,
	                    XMMSV_TYPE_INT32,
	                    time (NULL)-((xmms_main_t*)object)->starttime);

	xmms_object_unref (object);

	exit (EXIT_SUCCESS);
}


/**
 * @internal Function to respond to the 'quit' command sent from a client
 */
static void
xmms_main_client_quit (xmms_object_t *object, xmms_error_t *error)
{
	/*
	 * to be able to return from this method
	 * we add a timeout that will kill the server
	 * very "ugly"
	 */
	g_timeout_add (1, kill_server, object);
}

static void
install_scripts (const gchar *into_dir)
{
	GDir *dir;
	GError *err = NULL;
	gchar path[XMMS_PATH_MAX];
	const gchar *f;
	gchar *s;

	s = strrchr (into_dir, G_DIR_SEPARATOR);
	if (!s)
		return;

	s++;

	g_snprintf (path, XMMS_PATH_MAX, "%s/scripts/%s", SHAREDDIR, s);
	xmms_log_info ("Installing scripts from %s", path);
	dir = g_dir_open (path, 0, &err);
	if (!dir) {
		xmms_log_error ("Global script directory not found");
		return;
	}

	while ((f = g_dir_read_name (dir))) {
		gchar *source = g_strdup_printf ("%s/%s", path, f);
		gchar *dest = g_strdup_printf ("%s/%s", into_dir, f);
		if (!xmms_symlink_file (source, dest)) {
			g_free (source);
			g_free (dest);
			break;
		}
		g_free (source);
		g_free (dest);
	}

	g_dir_close (dir);
}

/**
 * Just print version and quit
 */
static void
print_version (void)
{
	printf ("XMMS2 version " XMMS_VERSION "\n");
	printf ("Copyright (C) 2003-2012 XMMS2 Team\n");
	printf ("This is free software; see the source for copying conditions.\n");
	printf ("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A\n");
	printf ("PARTICULAR PURPOSE.\n");
	printf (" Using glib version %d.%d.%d (compiled against "
	        G_STRINGIFY (GLIB_MAJOR_VERSION) "."
	        G_STRINGIFY (GLIB_MINOR_VERSION) "."
	        G_STRINGIFY (GLIB_MICRO_VERSION) ")\n",
	        glib_major_version,
	        glib_minor_version,
	        glib_micro_version);

	exit (EXIT_SUCCESS);
}

/**
 * The xmms2 daemon main initialisation function
 */
int
main (int argc, char **argv)
{
	xmms_output_plugin_t *o_plugin;
	xmms_config_property_t *cv;
	xmms_main_t *mainobj;
	int loglevel = 1;
	gchar default_path[XMMS_PATH_MAX + 16], *tmp;
	gboolean verbose = FALSE;
	gboolean quiet = FALSE;
	gboolean version = FALSE;
	gboolean runasroot = FALSE;
	gboolean showhelp = FALSE;
	const gchar *outname = NULL;
	const gchar *ipcpath = NULL;
	gchar *uuid, *ppath = NULL;
	int status_fd = -1;
	GOptionContext *context = NULL;
	GError *error = NULL;

	setlocale (LC_ALL, "");

	/**
	 * The options that the server accepts.
	 */
	GOptionEntry opts[] = {
		{"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Increase verbosity", NULL},
		{"quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet, "Decrease verbosity", NULL},
		{"version", 'V', 0, G_OPTION_ARG_NONE, &version, "Print version", NULL},
		{"output", 'o', 0, G_OPTION_ARG_STRING, &outname, "Use 'x' as output plugin", "<x>"},
		{"ipc-socket", 'i', 0, G_OPTION_ARG_FILENAME, &ipcpath, "Listen to socket 'url'", "<url>"},
		{"plugindir", 'p', 0, G_OPTION_ARG_FILENAME, &ppath, "Search for plugins in directory 'foo'", "<foo>"},
		{"conf", 'c', 0, G_OPTION_ARG_FILENAME, &conffile, "Specify alternate configuration file", "<file>"},
		{"status-fd", 's', 0, G_OPTION_ARG_INT, &status_fd, "Specify a filedescriptor to write to when started", "fd"},
		{"yes-run-as-root", 0, 0, G_OPTION_ARG_NONE, &runasroot, "Give me enough rope to shoot myself in the foot", NULL},
		{"show-help", 'h', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &showhelp, "Use --help or -? instead", NULL},
		{NULL}
	};

	/** Check that we are running against the correct glib version */
	if (glib_major_version != GLIB_MAJOR_VERSION ||
	    glib_minor_version < GLIB_MINOR_VERSION) {
		g_print ("xmms2d is build against version %d.%d,\n"
		         "but is (runtime) linked against %d.%d.\n"
		         "Refusing to start.\n",
		         GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION,
		         glib_major_version, glib_minor_version);
		exit (EXIT_FAILURE);
	}

	xmms_signal_block ();

	context = g_option_context_new ("- XMMS2 Daemon");
	g_option_context_add_main_entries (context, opts, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error) || error) {
		g_print ("Error parsing options: %s\n", error->message);
		g_clear_error (&error);
		exit (EXIT_FAILURE);
	}
	if (showhelp) {
#if GLIB_CHECK_VERSION(2,14,0)
		g_print ("%s", g_option_context_get_help (context, TRUE, NULL));
		exit (EXIT_SUCCESS);
#else
		g_print ("Please use --help or -? for help\n");
		exit (EXIT_FAILURE);
#endif
	}
	g_option_context_free (context);

	if (argc != 1) {
		g_print ("There were unknown options, aborting!\n");
		exit (EXIT_FAILURE);
	}

	if (xmms_checkroot ()) {
		if (runasroot) {
			g_print ("***************************************\n");
			g_print ("Warning! You are running XMMS2D as root, this is a bad idea!\nBut I'll allow it since you asked nicely.\n");
			g_print ("***************************************\n\n");
		} else {
			g_print ("PLEASE DON'T RUN XMMS2D AS ROOT!\n\n(if you really must, read the help)\n");
			exit (EXIT_FAILURE);
		}
	}

	if (verbose) {
		loglevel++;
	} else if (quiet) {
		loglevel--;
	}

	if (version) {
		print_version ();
	}

	g_thread_init (NULL);

	g_random_set_seed (time (NULL));

	xmms_log_init (loglevel);
	xmms_ipc_init ();

	load_config ();

	cv = xmms_config_property_register ("core.logtsfmt",
	                                    "%H:%M:%S ",
	                                    NULL, NULL);

	xmms_log_set_format (xmms_config_property_get_string (cv));

	xmms_fallback_ipcpath_get (default_path, sizeof (default_path));

	cv = xmms_config_property_register ("core.ipcsocket",
	                                    default_path,
	                                    on_config_ipcsocket_change,
	                                    NULL);

	if (!ipcpath) {
		/*
		 * if not ipcpath is specifed on the cmd line we
		 * grab it from the config
		 */
		ipcpath = xmms_config_property_get_string (cv);
	}

	if (!xmms_ipc_setup_server (ipcpath)) {
		xmms_ipc_shutdown ();
		xmms_log_fatal ("IPC failed to init!");
	}

	if (!xmms_plugin_init (ppath)) {
		exit (EXIT_FAILURE);
	}


	mainobj = xmms_object_new (xmms_main_t, xmms_main_destroy);

	mainobj->medialib_object = xmms_medialib_init ();
	mainobj->colldag_object = xmms_collection_init (mainobj->medialib_object);
	mainobj->mediainfo_object = xmms_mediainfo_reader_start (mainobj->medialib_object);
	mainobj->playlist_object = xmms_playlist_init (mainobj->medialib_object,
	                                               mainobj->colldag_object);

	uuid = xmms_medialib_uuid (mainobj->medialib_object);
	mainobj->collsync_object = xmms_coll_sync_init (uuid,
	                                                mainobj->colldag_object,
	                                                mainobj->playlist_object);
	g_free (uuid);
	mainobj->plsupdater_object = xmms_playlist_updater_init (mainobj->playlist_object);

	mainobj->xform_object = xmms_xform_object_init ();
	mainobj->bindata_object = xmms_bindata_init ();

	/* find output plugin. */
	cv = xmms_config_property_register ("output.plugin",
	                                    XMMS_OUTPUT_DEFAULT,
	                                    change_output, mainobj);

	if (outname) {
		xmms_config_property_set_data (cv, outname);
	}

	outname = xmms_config_property_get_string (cv);
	xmms_log_info ("Using output plugin: %s", outname);
	o_plugin = (xmms_output_plugin_t *) xmms_plugin_find (XMMS_PLUGIN_TYPE_OUTPUT, outname);
	if (!o_plugin) {
		xmms_log_error ("Baaaaad output plugin, try to change the"
		                "output.plugin config variable to something useful");
	}

	mainobj->output_object = xmms_output_new (o_plugin,
	                                          mainobj->playlist_object,
	                                          mainobj->medialib_object);
	if (!mainobj->output_object) {
		xmms_log_fatal ("Failed to create output object!");
	}

	mainobj->visualization_object = xmms_visualization_new (mainobj->output_object);

	if (status_fd != -1) {
		write (status_fd, "+", 1);
	}

	xmms_signal_init (XMMS_OBJECT (mainobj));

	xmms_main_register_ipc_commands (XMMS_OBJECT (mainobj));

	/* Save the time we started in order to count uptime */
	mainobj->starttime = time (NULL);

	/* Dirty hack to tell XMMS_PATH a valid path */
	g_strlcpy (default_path, ipcpath, sizeof (default_path));

	tmp = strchr (default_path, ';');
	if (tmp) {
		*tmp = '\0';
	}

	g_setenv ("XMMS_PATH", default_path, TRUE);

	/* Also put the full path for clients that understands */
	g_setenv("XMMS_PATH_FULL", ipcpath, TRUE);

	tmp = XMMS_BUILD_PATH ("shutdown.d");
	cv = xmms_config_property_register ("core.shutdownpath",
	                                    tmp, NULL, NULL);
	g_free (tmp);

	tmp = XMMS_BUILD_PATH ("startup.d");
	cv = xmms_config_property_register ("core.startuppath",
	                                    tmp, NULL, NULL);
	g_free (tmp);

	/* Startup dir */
	do_scriptdir (xmms_config_property_get_string (cv), "start");

	mainloop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (mainloop);

	return EXIT_SUCCESS;
}

/** @} */
