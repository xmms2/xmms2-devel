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

#include "xmms2_client.h"

static void mlib_add (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_loadall (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_searchadd (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_search (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_query (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_list (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_save (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_load (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_import (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_export (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_addpath (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_rehash (xmmsc_connection_t *conn, int argc, char **argv);

cmds mlib_commands[] = {
	{ "add", "[url] - Add 'url' to medialib", mlib_add },
	{ "loadall", "Load everything from the mlib to the playlist", mlib_loadall },
	{ "searchadd", "[artist=Dismantled] - Search for, and add songs to playlist", mlib_searchadd },
	{ "search", "[artist=Dismantled] - Search for songs matching criteria", mlib_search },
	{ "query", "[\"raw sql statement\"] - Send a raw SQL statement to the mlib", mlib_query },
	{ "list_playlist", "[playlistname] - List 'playlistname' stored in medialib", mlib_playlist_list },
	{ "save_playlist", "[playlistname] - Save current playlist to 'playlistname'", mlib_playlist_save },
	{ "load_playlist", "[playlistname] - Load 'playlistname' stored in medialib", mlib_playlist_load },
	{ "import_playlist", "[name] [filename] - Import playlist from file", mlib_playlist_import },
	{ "export_playlist", "[playlistname] [mimetype] - Export playlist", mlib_playlist_export },
	{ "addpath", "[path] - Import metadata from all media files under 'path'", mlib_addpath },
	{ "rehash", "Force the medialib to check whether its data is up to date", mlib_rehash },

	{ NULL, NULL, NULL },
};

static void
mlib_add (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	gint i;

	for (i = 3; argv[i]; i++) {
		char *url = format_url (argv[i]);
		if (!url) return;
		res = xmmsc_medialib_add_entry (conn, url);
		free (url);
		xmmsc_result_wait (res);
		printf ("Added %s to medialib\n", argv[i]);
		xmmsc_result_unref (res);
	}
}

static void
mlib_loadall (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	res = xmmsc_medialib_add_to_playlist (conn, "select id from Media where key='url'");
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);
}

static void
mlib_searchadd (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	char query[1024];
	char **s;
		
	if (!argv[3])
		print_error ("expected key=value");

	s = g_strsplit (argv[3], "=", 0);

	if (!s || !s[0] || !s[1])
		print_error ("expected key=value");

	g_snprintf (query, 1023, "select id from Media where LOWER(key)=LOWER('%s') and LOWER(value) like LOWER('%s')",s[0],s[1]);
	print_info ("%s", query);
	res = xmmsc_medialib_add_to_playlist (conn, query);
	xmmsc_result_wait (res);
	xmmsc_result_unref (res);
}

static void
mlib_search (xmmsc_connection_t *conn, int argc, char **argv)
{
	char query[1024];
	char **s;
	xmmsc_result_t *res;
	GList *n = NULL;

	if (argc < 4) {
		print_error ("Supply a select statement");
	}

	s = g_strsplit (argv[3], "=", 0);
		
	g_snprintf (query, sizeof (query), "SELECT id FROM Media WHERE LOWER(key)=LOWER('%s') and LOWER(value)=LOWER('%s')", s[0], s[1]);
	
	res = xmmsc_medialib_select (conn, query);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	for (; xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		char *id;
			
		xmmsc_result_get_dict_entry (res, "id", &id);
		if (!id)
			print_error ("broken resultset");

		n = g_list_prepend (n, id);
	}

	format_pretty_list (conn, n);
	g_list_free (n);
	xmmsc_result_unref (res);
}

static void
mlib_query (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply a query");
	}

	res = xmmsc_medialib_select (conn, argv[3]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	for (; xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		xmmsc_result_dict_foreach (res, print_hash, NULL);
	}

	xmmsc_result_unref (res);
}

static void
mlib_playlist_list (xmmsc_connection_t *conn, int argc, char **argv)
{
	char query[1024];
	GList *n = NULL;
	char *id;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}

	g_snprintf (query, sizeof (query), "SELECT id FROM Playlist WHERE name='%s'", argv[3]);

	res = xmmsc_medialib_select (conn, query);
	xmmsc_result_wait (res);

	/* yes, result is a hashlist,
	   but there should only be one entry */
	xmmsc_result_get_dict_entry (res, "id", &id);
	if (!id) 
		print_error ("No such playlist!");

	g_snprintf (query, sizeof (query), "SELECT entry FROM Playlistentries WHERE playlist_id = %s", id);
	xmmsc_result_unref (res);

	res = xmmsc_medialib_select (conn, query);
	xmmsc_result_wait (res);

	for (; xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		gchar *entry;
		xmmsc_result_get_dict_entry (res, "entry", &entry);
		if (!entry) 
			print_error ("No such playlist!");
		if (g_strncasecmp (entry, "mlib", 4) == 0) {
			char *p = entry+7;
			n = g_list_prepend (n, p);
		}
	}

	n = g_list_reverse (n);
	format_pretty_list (conn, n);
	g_list_free (n);
	xmmsc_result_unref (res);

}

static void
mlib_playlist_save (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}

	res = xmmsc_medialib_playlist_save_current (conn, argv[3]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static void
mlib_playlist_load (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}

	res = xmmsc_medialib_playlist_load (conn, argv[3]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static void
mlib_playlist_import (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	char *url;

	if (argc < 5) {
		print_error ("Supply a playlist name and url");
	}

	url = format_url (argv[4]);

	res = xmmsc_medialib_playlist_import (conn, argv[3], url);

	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static void
mlib_playlist_export (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	char *file;
	char *mime;

	if (argc < 5) {
		print_error ("Supply a playlist name and a mimetype");
	}

	if (strcasecmp (argv[4], "m3u") == 0) {
		mime = "audio/mpegurl";
	} else if (strcasecmp (argv[4], "pls") == 0) {
		mime = "audio/x-scpls";
	} else if (strcasecmp (argv[4], "html") == 0) {
		mime = "text/html";
	} else {
		mime = argv[4];
	}

	res = xmmsc_medialib_playlist_export (conn, argv[3], mime);

	xmmsc_result_wait (res);
	
	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	if (!xmmsc_result_get_string (res, &file))
		print_error ("broken resultset!");

	fwrite (file, strlen (file), 1, stdout);

	xmmsc_result_unref (res);
}

static void
mlib_addpath (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	if (argc < 4) {
		print_error ("Supply a path to add!");
	}
	res = xmmsc_medialib_path_import (conn, argv[3]);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

static void
mlib_rehash (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	unsigned int id = 0;
	if (argc < 4) {
		print_info ("Rehashing whole medialib!");
	} else {
		id = atoi (argv[3]);
	}
	res = xmmsc_medialib_rehash (conn, id);
	xmmsc_result_wait (res);
	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

static void
mlib_help (void) {
	gint i;

	print_info ("Available medialib commands:");

	for (i = 0; mlib_commands[i].name; i++) {
		print_info ("  %s\t %s", mlib_commands[i].name, mlib_commands[i].help);
	}
}

void
cmd_mlib (xmmsc_connection_t *conn, int argc, char **argv)
{
	gint i;
	if (argc < 3) {
		mlib_help();
		return;
	}

	for (i = 0; mlib_commands[i].name; i++) {

		if (g_strcasecmp (mlib_commands[i].name, argv[2]) == 0) {
			mlib_commands[i].func (conn, argc, argv);
			return;
		}

	}

	mlib_help();
	print_error ("Unrecognised mlib command: %s", argv[2]);
}
