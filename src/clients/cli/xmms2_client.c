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

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-glib.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

typedef struct {
	char *name;
	char *help;
	void (*func) (xmmsc_connection_t *conn, int argc, char **argv);
} cmds;

/**
 * Utils
 */

void
print_error (const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	
	va_start (ap, fmt);
	vsnprintf (buf, 1024, fmt, ap);
	va_end (ap);

	printf ("ERROR: %s\n", buf);

	exit (-1);
}

void
print_info (const char *fmt, ...)
{
	char buf[8096];
	va_list ap;
	
	va_start (ap, fmt);
	vsnprintf (buf, 8096, fmt, ap);
	va_end (ap);

	printf ("%s\n", buf);
}

/**
 * here comes all the cmd callbacks
 */

static void
cmd_add (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i;

	if (argc < 3) {
		print_error ("Need a filename to add");
	}

	for (i = 2; argv[i]; i++) {
		char url[4096];

		if (!strchr (argv[i], ':')) {
			/* OK, so this is NOT an valid URL */

			if (!g_file_test (argv[i], G_FILE_TEST_IS_REGULAR))
				continue;
			
			if (argv[i][0] == '/') {
				g_snprintf (url, 4096, "file://%s", argv[i]);
			} else {
				char *cwd = g_get_current_dir ();
				g_snprintf (url, 4096, "file://%s/%s", cwd, argv[i]);
				g_free (cwd);
			}
		} else {
			g_snprintf (url, 4096, "%s", argv[i]);
		}
		
		print_info ("Adding %s", url);
		xmmsc_playlist_add (conn, xmmsc_encode_path (url));
	}
}

static void
cmd_clear (xmmsc_connection_t *conn, int argc, char **argv)
{

	xmmsc_playback_stop (conn);
	xmmsc_playlist_clear (conn);

}

static void
cmd_shuffle (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_playlist_shuffle (conn);
}

static void
cmd_remove (xmmsc_connection_t *conn, int argc, char **argv)
{
	int i;

	if (argc < 3) {
		print_error ("Remove needs a ID to be removed");
	}

	for (i = 2; argv[i]; i++) {
		xmmsc_playlist_remove (conn, atoi (argv[i]));
	}
}


static void
cmd_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	unsigned int *list;
	int i;
	int id;

	list = xmmscs_playlist_list (conn);

	if (*list == -1)
		return;

	id = xmmscs_playback_current_id (conn);

	for (i = 0; list[i]; i++) {
		x_hash_t *tab;
		char line[80];
		
		tab = xmmscs_playlist_get_mediainfo (conn, list[i]);

		memset (line, '\0', 80);

		if (!x_hash_lookup (tab, "title")) {
			xmmsc_entry_format (line, 80, "%f (%m:%s)", tab);
		} else {
			xmmsc_entry_format (line, 80, "%a - %t (%m:%s)", tab);
		}

		if (id == list[i]) {
			print_info ("->[%d] %s", list[i], line);
		} else {
			print_info ("  [%d] %s", list[i], line);
		}
		
	}
	

}
	
static void
cmd_play (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_playback_start (conn);
}

static void
cmd_stop (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_playback_stop (conn);
}

static void
cmd_pause (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_playback_pause (conn);

}

static void
cmd_next (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_play_next (conn);
}

static void
cmd_prev (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_play_prev (conn);
}

static void
cmd_seek (xmmsc_connection_t *conn, int argc, char **argv)
{
	unsigned int seconds;

	seconds = atoi (argv[2]) * 1000;

	xmmsc_playback_seek_ms (conn, seconds);
}

static void
cmd_quit (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_quit (conn);
}

static void
cmd_config (xmmsc_connection_t *conn, int argc, char **argv)
{
	if (argc < 4) {
		print_error ("You need to specify a configkey and a value");
	}

	xmmsc_configval_set (conn, argv[2], argv[3]);
}

static void
cmd_jump (xmmsc_connection_t *conn, int argc, char **argv)
{

	if (argc < 3) {
		print_error ("You'll need to specify a ID to jump to.");
	}

	xmmsc_playback_stop (conn);
	xmmsc_playlist_jump (conn, atoi (argv[2]));
	xmmsc_playback_start (conn);
}

/**
 * Defines all available commands.
 */

cmds commands[] = {
	/* Playlist managment */
	{ "add", "adds a URL to the playlist", cmd_add },
	{ "clear", "clears the playlist and stops playback", cmd_clear },
	{ "shuffle", "shuffles the playlist", cmd_shuffle },
	{ "remove", "removes something from the playlist", cmd_remove },
	{ "list", "lists the playlist", cmd_list },
	
	/* Playback managment */
	{ "play", "starts playback", cmd_play },
	{ "stop", "stops playback", cmd_stop },
	{ "pause", "pause playback", cmd_pause },
	{ "next", "play next song", cmd_next },
	{ "prev", "play previous song", cmd_prev },
	{ "seek", "seek to a specific place in current song", cmd_seek },
	{ "jump", "take a leap in the playlist", cmd_jump },
//	{ "move", "move a entry in the playlist", cmd_move },

	{ "quit", "make the server quit", cmd_quit },

	{ "config", "set a config value", cmd_config },

	{ NULL, NULL, NULL },
};


int
main (int argc, char **argv)
{
	xmmsc_connection_t *connection;
	char *path;
	int i;

	connection = xmmsc_init ();

	if (!connection) {
		print_error ("Could not init xmmsc_connection, this is a memory problem, fix your os!");
	}

	
	path = g_strdup_printf ("unix:path=/tmp/xmms-dbus-%s", g_get_user_name ());
	if (!xmmsc_connect (connection, path)) {
		print_error ("Could not connect to xmms2d: %s", xmmsc_get_last_error (connection));
	}

	if (argc < 2) {

		xmmsc_deinit (connection);


		print_info ("Available commands:");
		
		for (i = 0; commands[i].name; i++) {
			print_info ("  %s - %s", commands[i].name, commands[i].help);
		}
		

		exit (0);

	}

	for (i = 0; commands[i].name; i++) {

		if (g_strcasecmp (commands[i].name, argv[1]) == 0) {
			commands[i].func (connection, argc, argv);
			xmmsc_deinit (connection);
			exit (0);
		}

	}

	xmmsc_deinit (connection);
	
	print_error ("Could not find any command called %s", argv[1]);

	return -1;

}

