/** @file 
 * This file controls XMMS2 mainloop.
 */

#include <glib.h>

#include "xmms/plugin.h"
#include "xmms/plugin_int.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/config.h"
#include "xmms/playlist.h"
#include "xmms/unixsignal.h"
#include "xmms/util.h"
#include "xmms/core.h"
#include "xmms/medialib.h"
#include "xmms/output.h"
#include "xmms/output_int.h"
#include "xmms/xmms.h"
#include "xmms/effect.h"
#include "xmms/dbus.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

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
		if (g_file_test (XMMS_CONFIG_SYSTEMWIDE, G_FILE_TEST_EXISTS)) {
			if (!xmms_config_init (XMMS_CONFIG_SYSTEMWIDE)) {
				XMMS_DBG ("XMMS was unable to parse configfile %s", filename);
				exit (EXIT_FAILURE);
			}

			if (!g_file_test (configdir, G_FILE_TEST_IS_DIR)) {
				mkdir (configdir, 0755);
			}

			if (!xmms_config_save (filename)) {
				XMMS_DBG ("Could't write file %s!", filename);
				exit (EXIT_FAILURE);
			}

			return TRUE;
		} else {
			XMMS_DBG ("XMMS was unable to find systemwide configfile %s", XMMS_CONFIG_SYSTEMWIDE);
			exit (EXIT_FAILURE);
		}
	}
	return FALSE;
}


int
main (int argc, char **argv)
{
	xmms_plugin_t *o_plugin;
	xmms_config_value_t *cv;
	int opt;
	int verbose = 0;
	sigset_t signals;
	xmms_playlist_t *playlist;
	const gchar *outname = NULL;
	gboolean daemonize = FALSE;
	const gchar *path;
	gchar *ppath = NULL;
	pid_t ppid=0;

	memset (&signals, 0, sizeof (sigset_t));
	sigaddset (&signals, SIGHUP);
	sigaddset (&signals, SIGTERM);
	sigaddset (&signals, SIGINT);
	pthread_sigmask (SIG_BLOCK, &signals, NULL);

	
	while (42) {
		opt = getopt (argc, argv, "dvVo:p:");

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
			exit (caught!=SIGUSR1);
		}
		setsid();
		if (fork ()) exit(0);
		xmms_log_daemonize ();
	}

	g_thread_init (NULL);

	xmms_log_initialize ("xmmsd");

	xmms_core_init ();
	
	parse_config ();

	if (!xmms_plugin_init (ppath))
		return 1;
	
	playlist = xmms_playlist_init ();
	xmms_core_set_playlist (playlist);
	if (optind) {
		while (argv[optind]) {
			gchar nurl[XMMS_MAX_URI_LEN];
			if (!strchr (argv[optind], ':')) {
				XMMS_DBG ("No protocol, assuming file");
				if (argv[optind][0] == '/') {
					g_snprintf (nurl, XMMS_MAX_URI_LEN, "file://%s", argv[optind]);
				} else {
					gchar *cwd = g_get_current_dir ();
					g_snprintf (nurl, XMMS_MAX_URI_LEN, "file://%s/%s", cwd, argv[optind]);
					g_free (cwd);
				}
			} else {
				g_snprintf (nurl, XMMS_MAX_URI_LEN, "%s", argv[optind]);
			}
				
			XMMS_DBG ("Adding url %s to playlist", nurl);
			xmms_playlist_add (playlist, xmms_playlist_entry_new (nurl), XMMS_PLAYLIST_APPEND);
			optind++;
		}
	}

	XMMS_DBG ("Playlist contains %d entries", xmms_playlist_entries_total (playlist));

	cv = xmms_config_value_register ("core.outputplugin", "oss", NULL, NULL);
	outname = xmms_config_value_string_get (cv);
	XMMS_DBG ("output = %s", outname);

	o_plugin = xmms_output_find_plugin (outname);
	g_return_val_if_fail (o_plugin, -1);
	{
		xmms_output_t *output = xmms_output_new (o_plugin);
		g_return_val_if_fail (output, -1);
		
		xmms_core_output_set (output);

		xmms_output_start (output);
	}

	path = g_strdup_printf ("unix:path=/tmp/xmms-dbus-%s", 
				g_get_user_name ());
	cv = xmms_config_value_register ("core.dbuspath", path, NULL, NULL);
	path = xmms_config_value_string_get (cv);

	xmms_dbus_init (path);

	xmms_signal_init ();

	xmms_core_start ();

	if (ppid) { /* signal that we are inited */
		kill (ppid, SIGUSR1);
	}

	mainloop = g_main_loop_new (NULL, FALSE);

	g_main_loop_run (mainloop);

	return 0;
}
