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

#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cmd_playback.h"
#include "cmd_mlib.h"
#include "cmd_coll.h"
#include "cmd_config.h"
#include "cmd_status.h"
#include "cmd_pls.h"
#include "cmd_other.h"
#include "common.h"


/**
 * Function prototypes
 */
static void cmd_help (xmmsc_connection_t *conn, gint argc, gchar **argv);


/**
 * Globals
 */
gchar *statusformat = NULL;
gchar *listformat = NULL;
GHashTable *config = NULL;
gchar defaultconfig[] = "ipcpath=NULL\nstatusformat=${artist} - ${title}\nlistformat=${artist} - ${title} (${minutes}:${seconds})\nautostart=true\nrunnycli=false\niknowoldcliisdeprecatedandwillgoaway=false";


/**
 * Defines all available commands.
 */
cmds commands[] = {
	/* Playlist managment */
	{ "add", "adds a URL to the playlist", cmd_add },
	{ "addarg", "adds one URL with arguments to the playlist", cmd_addarg },
	{ "addid", "adds a Medialib id to the playlist", cmd_addid },
	{ "insert", "inserts one URL at a specific position", cmd_insert },
	{ "insertid", "inserts one Medialib id at a specific position", cmd_insertid },
	{ "radd", "adds a directory recursively to the playlist", cmd_radd },
	{ "rinsert", "inserts a directory recursively at a specific position", cmd_rinsert },
	{ "clear", "clears the playlist", cmd_clear },
	{ "shuffle", "shuffles the playlist", cmd_shuffle },
	{ "sort", "sort the playlist; use a space delimiter for multiple properties", cmd_sort },
	{ "remove", "removes something from the playlist", cmd_remove },
	{ "list", "lists the playlist", cmd_list },
	{ "addpls", "Adds the contents of a playlist file to the playlist", cmd_addpls },

	/* Playback managment */
	{ "play", "starts playback", cmd_play },
	{ "stop", "stops playback", cmd_stop },
	{ "toggleplay", "toggles playback status between play/pause", cmd_toggleplay },
	{ "pause", "pause playback", cmd_pause },
	{ "next", "play next song", cmd_next },
	{ "prev", "play previous song", cmd_prev },
	{ "seek", "seek to a specific place in current song", cmd_seek },
	{ "jump", "take a leap in the playlist", cmd_jump },
	{ "move", "move a entry in the playlist", cmd_move },
	{ "volume", "set volume for a channel", cmd_volume },
	{ "volume_list", "list volume levels for each channel", cmd_volume_list },

	{ "mlib", "medialib manipulation - type 'xmms2 mlib' for more extensive help", cmd_mlib },
	{ "playlist", "playlist manipulation - type 'xmms2 playlist' for more extensive help", cmd_playlist },
	{ "coll", "collection manipulation - type 'xmms2 coll' for more extensive help", cmd_coll },

	{ "browse", "browse server file lists", cmd_browse },

	{ "status", "go into status mode", cmd_status },
	{ "info", "information about current entry", cmd_info },
	{ "current", "formatted information about the current entry", cmd_current },
	{ "config", "set a config value", cmd_config },
	{ "config_list", "list all config values", cmd_config_list },
	{ "plugin_list", "list all plugins loaded in the server", cmd_plugin_list },
	{ "stats", "get statistics from server", cmd_stats },
	{ "quit", "make the server quit", cmd_quit },
	{ "help", "print help about a command", cmd_help},

	{ NULL, NULL, NULL },
};


/**
 * Config functions.
 */
static GHashTable *
parse_config (const gchar *buffer)
{
	gchar **split;
	gint i;
	GHashTable *config;

	config = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	split = g_strsplit (buffer, "\n", 0);
	for (i = 0; split && split[i]; i++) {
		gchar **s;

		s = g_strsplit (split[i], "=", 2);
		if (s && s[0] && s[1]) {
			if (g_ascii_strcasecmp (s[1], "NULL") == 0) {
				g_hash_table_insert (config, g_strdup (s[0]), NULL);
			} else {
				g_hash_table_insert (config, g_strdup (s[0]), g_strdup (s[1]));
			}
		}
		g_strfreev (s);
	}
	g_strfreev (split);

	return config;
}

static gchar *
get_config_dir (void) {
	gchar userconf[PATH_MAX];

	xmmsc_userconfdir_get (userconf, PATH_MAX);
	return g_build_path (G_DIR_SEPARATOR_S, userconf, "clients", NULL);
}

static gchar *
get_config_path (void) {
	gchar userconf[PATH_MAX];

	xmmsc_userconfdir_get (userconf, PATH_MAX);
	return g_build_path (G_DIR_SEPARATOR_S, userconf, "clients", "cli.conf",
	                     NULL);
}

static GHashTable *
read_config (void)
{
	GHashTable *config;
	gchar *buffer, *file;
	gint read_bytes = 0;
	struct stat st;
	FILE *fp;

	file = get_config_path ();

	if (!g_file_test (file, G_FILE_TEST_EXISTS)) {
		gchar *dir = get_config_dir ();
		g_mkdir_with_parents (dir, 0755);
		g_free (dir);

		fp = fopen (file, "w+");
		if (fp) {
			fwrite (defaultconfig, strlen (defaultconfig), 1, fp);
			fclose (fp);
		} else {
			print_info ("Could not create configfile: %s\nMake sure you have write permissions to that location.", file);
		}
	}

	if (g_file_test (file, G_FILE_TEST_EXISTS)) {
		fp = fopen (file, "r");
		if (!fp) {
			print_error ("Could not open configfile %s", file);
		}
		g_free (file);

		if (fstat (fileno (fp), &st) == -1) {
			print_error ("fstat");
		}

		buffer = g_malloc0 (st.st_size + 1);

		while (read_bytes < st.st_size) {
			guint ret = fread (buffer + read_bytes,
			                   st.st_size - read_bytes, 1, fp);

			if (ret == 0) {
				break;
			}

			read_bytes += ret;
			g_assert (read_bytes >= 0);
		}

		config = parse_config (buffer);

		g_free (buffer);
	} else {
		g_free (file);
		config = parse_config (defaultconfig);
	}

	return config;
}


static void
free_config (void)
{
	if (config) {
		g_hash_table_destroy (config);
	}
}


/**
 * Usage
 */
static void
cmd_help (xmmsc_connection_t *conn, gint argc, gchar **argv)
{

	gint i;
	if (argc == 2) {
		/* print help message for all commands */
		print_info ("Available commands:");
		for (i = 0; commands[i].name; i++) {
			print_info ("  %s - %s", commands[i].name, commands[i].help);
		}
	} else if (argc == 3) {
		/* print help for specified command */
		for (i = 0; commands[i].name; i++) {
			if (g_ascii_strcasecmp (commands[i].name, argv[2]) == 0) {
				print_info ("  %s - %s", commands[i].name, commands[i].help);
			}
		}
	}
}


/**
 * Main
 */
gint
main (gint argc, gchar **argv)
{
	xmmsc_connection_t *connection;
	gchar *path;
	gchar *tmp, *tmp2;
	gint i, ret;
	void (*func) (xmmsc_connection_t *conn, int argc, char **argv) = NULL;

	setlocale (LC_ALL, "");

	config = read_config ();
	atexit (free_config);

	tmp = g_hash_table_lookup (config, "runnycli");
	if (tmp && !strcmp (tmp, "true")) {
		execvp ("nyxmms2", argv);
	}

	tmp = g_hash_table_lookup (config, "iknowoldcliisdeprecatedandwillgoaway");
	if (!tmp || strcmp (tmp, "true")) {
		tmp2 = get_config_path ();
		fprintf (stderr, "This program is deprecated and will be replaced by nyxmms2.\n"
			  " Consider setting runnycli to 'true' in config file to get future behaviour\n"
			  " (or set iknowoldcliisdeprecatedandwillgoaway to 'true' to hide this warning)\n"
			  " The config file is located at:\n    %s\n", tmp2);
		g_free (tmp2);
	}
	statusformat = g_hash_table_lookup (config, "statusformat");
	listformat = g_hash_table_lookup (config, "listformat");

	if (argc < 2) {
		print_info ("Available commands:");

		for (i = 0; commands[i].name; i++) {
			print_info ("  %s - %s", commands[i].name, commands[i].help);
		}

		exit (EXIT_SUCCESS);
	} else if (g_ascii_strcasecmp (argv[1], "help") == 0) {
		cmd_help (NULL, argc, argv);
		exit (EXIT_SUCCESS);
	}

	connection = xmmsc_init ("xmms2-cli");

	if (!connection) {
		print_error ("Could not init xmmsc_connection!");
	}

	path = getenv ("XMMS_PATH");
	if (!path) {
		path = g_hash_table_lookup (config, "ipcpath");
	}


	for (i = 0; commands[i].name; i++) {
		if (g_ascii_strcasecmp (commands[i].name, argv[1]) == 0) {
			func = commands[i].func;
		}
	}

	if (!func) {
		print_error ("Could not find any command called %s", argv[1]);
		exit (EXIT_FAILURE);
	}

	ret = xmmsc_connect (connection, path);
	if (!ret) {
		gboolean autostart = FALSE;
		gchar *tmp;

		tmp = g_hash_table_lookup (config, "autostart");
		if (tmp && !g_ascii_strcasecmp (tmp, "true")) {
		   autostart = TRUE;
		}

		if (autostart && g_ascii_strncasecmp (argv[1], "quit", 4) &&
		    (!path || !g_ascii_strncasecmp (path, "unix://", 7))) {
			if (!system ("xmms2-launcher")) {
				ret = xmmsc_connect (connection, path);
			}
		}
	}

	if (!ret) {
		print_error ("Could not connect to xmms2d: %s",
		             xmmsc_get_last_error (connection));
		xmmsc_unref (connection);
		exit (EXIT_FAILURE);
	}

	func (connection, argc, argv);
	xmmsc_unref (connection);

	return 0;
}
