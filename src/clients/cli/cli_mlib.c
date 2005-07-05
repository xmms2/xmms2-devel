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
static void mlib_queryadd (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_list (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_save (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_load (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_import (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_export (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_playlist_remove (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_addpath (xmmsc_connection_t *conn, int argc, char **argv);
static void mlib_rehash (xmmsc_connection_t *conn, int argc, char **argv);

cmds mlib_commands[] = {
	{ "add", "[url] - Add 'url' to medialib", mlib_add },
	{ "loadall", "Load everything from the mlib to the playlist", mlib_loadall },
	{ "searchadd", "[artist=Dismantled] ... - Search for, and add songs to playlist", mlib_searchadd },
	{ "search", "[artist=Dismantled] ... - Search for songs matching criteria", mlib_search },
	{ "query", "[\"raw sql statement\"] - Send a raw SQL statement to the mlib", mlib_query },
	{ "queryadd", "[\"raw sql statement\"] - Send a raw SQL statement to the mlib and add all entries to playlist", mlib_queryadd },
	{ "list_playlist", "[playlistname] - List 'playlistname' stored in medialib", mlib_playlist_list },
	{ "save_playlist", "[playlistname] - Save current playlist to 'playlistname'", mlib_playlist_save },
	{ "load_playlist", "[playlistname] - Load 'playlistname' stored in medialib", mlib_playlist_load },
	{ "import_playlist", "[name] [filename] - Import playlist from file", mlib_playlist_import },
	{ "export_playlist", "[playlistname] [mimetype] - Export playlist", mlib_playlist_export },
	{ "remove_playlist", "[playlistname] - Remove a playlist", mlib_playlist_remove },
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

char *
mlib_query_from_args (int argc, char **argv) {
	gchar *query;
	char **s;
	guint i;
	xmmsc_query_attribute_t *query_spec;

	if (argc < 4) {
		print_error ("Supply a query");
	}		

	query_spec = (xmmsc_query_attribute_t*) malloc((argc-3)*sizeof(xmmsc_query_attribute_t));

	for (i=3; i < argc; i++) {
		s = g_strsplit (argv[i], "=", 0);
		if (!s[0] || !s[1]) {
			return NULL;
		}
		query_spec[i-3].key = xmmsc_sqlite_prepare_string(s[0]);
		query_spec[i-3].value = xmmsc_sqlite_prepare_string(s[1]);

		if (query_spec[i-3].key == NULL || query_spec[i-3].value == NULL) {
			free(query_spec[i-3].key);
			free(query_spec[i-3].value);
			while (i-3 > 0) {
				i--;
				free(query_spec[i-3].key);
				free(query_spec[i-3].value);
			}
			free(query_spec);
			return NULL;
		}

		g_strfreev(s);
	}	
		
	query = xmmsc_querygen_and(query_spec, argc-3);	
		
	free(query_spec);

	return query;
	
}

static void
mlib_queryadd (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply a query");
	}

	res = xmmsc_medialib_add_to_playlist (conn, argv[3]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static void
mlib_searchadd (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	char *query;
		
	query = mlib_query_from_args(argc, argv);
	if (query == NULL) {
		print_info("Unable to generate query");
		return;
	}

	print_info("%s", query);

	res = xmmsc_medialib_add_to_playlist (conn, query);
	free(query);

	xmmsc_result_wait (res);
	xmmsc_result_unref (res);
}


static void
mlib_search (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;
	GList *n = NULL;
	char *query;

	query = mlib_query_from_args(argc, argv);
	if (query == NULL) {
		print_info("Unable to generate query");
		return;
	}
	
	print_info ("%s", query);

	res = xmmsc_medialib_select (conn, query);
	xmmsc_result_wait (res);

	free(query);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	for (; xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		char *id;
			
		xmmsc_result_get_dict_entry_str (res, "id", &id);
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
	xmmsc_result_get_dict_entry_str (res, "id", &id);
	if (!id) 
		print_error ("No such playlist!");

	g_snprintf (query, sizeof (query), "SELECT entry FROM Playlistentries WHERE playlist_id = %s", id);
	xmmsc_result_unref (res);

	res = xmmsc_medialib_select (conn, query);
	xmmsc_result_wait (res);

	for (; xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		gchar *entry;
		xmmsc_result_get_dict_entry_str (res, "entry", &entry);
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
	if (!url) return;

	res = xmmsc_medialib_playlist_import (conn, argv[3], url);

	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}

static void
mlib_playlist_remove (xmmsc_connection_t *conn, int argc, char **argv)
{
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}

	res = xmmsc_medialib_playlist_remove (conn, argv[3]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	} else {
		print_info ("Playlist removed");
	}
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
	char rpath[PATH_MAX];
	if (argc < 4) {
		print_error ("Supply a path to add!");
	}
	
	if (!realpath (argv[3], rpath)) 
		return;
	if (!g_file_test (rpath, G_FILE_TEST_IS_DIR))
		return;

	res = xmmsc_medialib_path_import (conn, rpath);
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
