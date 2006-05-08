/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

#include "cmd_mlib.h"
#include "common.h"


/**
 * Function prototypes
 */
static void cmd_mlib_topsongs (xmmsc_connection_t *conn,
                               gint argc, gchar **argv);
static void cmd_mlib_set (xmmsc_connection_t *conn,
                          gint argc, gchar **argv);
static void cmd_mlib_add (xmmsc_connection_t *conn,
                          gint argc, gchar **argv);
static void cmd_mlib_loadall (xmmsc_connection_t *conn,
                              gint argc, gchar **argv);
static void cmd_mlib_queryadd (xmmsc_connection_t *conn,
                               gint argc, gchar **argv);
static void cmd_mlib_searchadd (xmmsc_connection_t *conn,
                                gint argc, gchar **argv);
static void cmd_mlib_search (xmmsc_connection_t *conn,
                             gint argc, gchar **argv);
static void cmd_mlib_query (xmmsc_connection_t *conn,
                            gint argc, gchar **argv);
static void cmd_mlib_playlist_list (xmmsc_connection_t *conn,
                                    gint argc, gchar **argv);
static void cmd_mlib_playlist_save (xmmsc_connection_t *conn,
                                    gint argc, gchar **argv);
static void cmd_mlib_playlist_load (xmmsc_connection_t *conn,
                                    gint argc, gchar **argv);
static void cmd_mlib_playlists_list (xmmsc_connection_t *conn,
                                     gint argc, gchar **argv);
static void cmd_mlib_playlist_import (xmmsc_connection_t *conn,
                                      gint argc, gchar **argv);
static void cmd_mlib_playlist_remove (xmmsc_connection_t *conn,
                                      gint argc, gchar **argv);
static void cmd_mlib_playlist_export (xmmsc_connection_t *conn,
                                      gint argc, gchar **argv);
static void cmd_mlib_addpath (xmmsc_connection_t *conn,
                              gint argc, gchar **argv);
static void cmd_mlib_rehash (xmmsc_connection_t *conn,
                             gint argc, gchar **argv);
static void cmd_mlib_remove (xmmsc_connection_t *conn,
                             gint argc, gchar **argv);

cmds mlib_commands[] = {
	{ "add", "[url] - Add 'url' to medialib", cmd_mlib_add },
	{ "loadall", "Load everything from the mlib to the playlist", cmd_mlib_loadall },
	{ "searchadd", "[artist=Dismantled] ... - Search for, and add songs to playlist", cmd_mlib_searchadd },
	{ "search", "[artist=Dismantled] ... - Search for songs matching criteria", cmd_mlib_search },
	{ "query", "[\"raw sql statement\"] - Send a raw SQL statement to the mlib", cmd_mlib_query },
	{ "queryadd", "[\"raw sql statement\"] - Send a raw SQL statement to the mlib and add all entries to playlist", cmd_mlib_queryadd },
	{ "list_playlist", "[playlistname] - List 'playlistname' stored in medialib", cmd_mlib_playlist_list },
	{ "save_playlist", "[playlistname] - Save current playlist to 'playlistname'", cmd_mlib_playlist_save },
	{ "load_playlist", "[playlistname] - Load 'playlistname' stored in medialib", cmd_mlib_playlist_load },
	{ "playlists_list", "List all available playlists", cmd_mlib_playlists_list },
	{ "import_playlist", "[name] [filename] - Import playlist from file", cmd_mlib_playlist_import },
	{ "export_playlist", "[playlistname] [mimetype] - Export playlist", cmd_mlib_playlist_export },
	{ "remove_playlist", "[playlistname] - Remove a playlist", cmd_mlib_playlist_remove },
	{ "addpath", "[path] - Import metadata from all media files under 'path'", cmd_mlib_addpath },
	{ "rehash", "Force the medialib to check whether its data is up to date", cmd_mlib_rehash },
	{ "remove", "Remove an entry from medialib", cmd_mlib_remove },
	{ "set", "[id, key, value, (source)] Set a property together with a medialib entry.", cmd_mlib_set },
	{ "topsongs", "list the most played songs", cmd_mlib_topsongs },
	{ NULL, NULL, NULL },
};


static void
cmd_mlib_help (void) {
	gint i;

	print_info ("Available medialib commands:");
	for (i = 0; mlib_commands[i].name; i++) {
		print_info ("  %s\t %s", mlib_commands[i].name,
		            mlib_commands[i].help);
	}
}


void
cmd_mlib (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;
	if (argc < 3) {
		cmd_mlib_help();
		return;
	}

	for (i = 0; mlib_commands[i].name; i++) {
		if (g_strcasecmp (mlib_commands[i].name, argv[2]) == 0) {
			mlib_commands[i].func (conn, argc, argv);
			return;
		}
	}

	cmd_mlib_help();
	print_error ("Unrecognised mlib command: %s", argv[2]);
}


void
cmd_info (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	guint id;

	if (argc > 2) {
		gint cnt;

		for (cnt = 2; cnt < argc; cnt++) {
			id = strtoul (argv[cnt], (gchar**) NULL, 10);

			res = xmmsc_medialib_get_info (conn, id);
			xmmsc_result_wait (res);

			if (xmmsc_result_iserror (res)) {
				print_error ("%s", xmmsc_result_get_error (res));
			}

			xmmsc_result_propdict_foreach (res, print_entry, NULL);
			xmmsc_result_unref (res);
		}

	} else {
		res = xmmsc_playback_current_id (conn);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}

		if (!xmmsc_result_get_uint (res, &id)) {
			print_error ("Broken resultset");
		}
		xmmsc_result_unref (res);
		
		res = xmmsc_medialib_get_info (conn, id);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}

		xmmsc_result_propdict_foreach (res, print_entry, NULL);
		xmmsc_result_unref (res);
	}
}


static void
cmd_mlib_topsongs (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	const gchar *query;
	GList *n = NULL;
	
	query = "select m.id as id, sum(l.value) as playsum from Log l left join Media m on l.id=m.id where m.key='url' group by l.id order by playsum desc limit 20";

	res = xmmsc_medialib_select (conn, query);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		gint id;

		if (!xmmsc_result_get_dict_entry_int32 (res, "id", &id)) {
			print_error ("Broken resultset");
		}

		n = g_list_prepend (n, XINT_TO_POINTER (id));
		xmmsc_result_list_next (res);
	}

	n = g_list_reverse (n);
	format_pretty_list (conn, n);
	g_list_free (n);

	xmmsc_result_unref (res);
}


static void
cmd_mlib_set (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gint id;

	if (argc < 6) {
		print_error ("usage: set [id] [key] [value] ([source])");
	}

	id = strtol (argv[3], NULL, 10);
	
	if (argc == 7) {
		res = xmmsc_medialib_entry_property_set_with_source (conn,
		                                                     id,
		                                                     argv[6],
		                                                     argv[4],
		                                                     argv[5]);
	} else {
		res = xmmsc_medialib_entry_property_set (conn, id, argv[4],
		                                         argv[5]);
	}
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);
}



static void
cmd_mlib_add (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gint i;

	for (i = 3; argv[i]; i++) {
		gchar *url;
		
		url = format_url (argv[i]);
		if (url) {
			res = xmmsc_medialib_add_entry (conn, url);
			xmmsc_result_wait (res);
			g_free (url);

			if (xmmsc_result_iserror (res)) {
				print_error ("%s", xmmsc_result_get_error (res));
			}

			print_info ("Added %s to medialib", argv[i]);
			xmmsc_result_unref (res);
		}
	}
}


static void
cmd_mlib_loadall (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *query;

	query = "select id from Media where key='url'";

	res = xmmsc_medialib_add_to_playlist (conn, query);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


static void
free_query_spec (xmmsc_query_attribute_t *specs, gint num_attribs)
{
	gint i;

	if (!specs) {
		g_assert (!num_attribs);
	} else {
		for (i = 0; i < num_attribs; i++) {
			g_free (specs[i].key);
			g_free (specs[i].value);
		}
		g_free (specs);
	}
}


static gchar *
mlib_query_from_args (gint argc, gchar **argv)
{
	gchar *query;
	gchar **s;
	guint i, num_attribs;
	xmmsc_query_attribute_t *query_spec;

	if (argc < 4) {
		print_error ("Supply a query");
	}

	num_attribs = argc - 3;
	query_spec = g_new0 (xmmsc_query_attribute_t, num_attribs);

	for (i = 0; i < num_attribs; i++) {
		s = g_strsplit (argv[i + 3], "=", 0);
		g_assert (s);

		if (!s[0] || !s[1]) {
			g_strfreev (s);
			free_query_spec (query_spec, num_attribs);

			return NULL;
		}

		query_spec[i].key = xmmsc_sqlite_prepare_string (s[0]);
		query_spec[i].value = xmmsc_sqlite_prepare_string (s[1]);
		g_strfreev (s);

		if (!query_spec[i].key || !query_spec[i].value) {
			free_query_spec (query_spec, num_attribs);

			return NULL;
		}
	}

	query = xmmsc_querygen_and (query_spec, num_attribs);
	free_query_spec (query_spec, num_attribs);

	return query;
}


static void
cmd_mlib_queryadd (xmmsc_connection_t *conn, gint argc, gchar **argv)
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
cmd_mlib_searchadd (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *query;

	query = mlib_query_from_args (argc, argv);
	if (query == NULL) {
		print_error ("Unable to generate query");
	}
	
	res = xmmsc_medialib_add_to_playlist (conn, query);
	xmmsc_result_wait (res);
	g_free (query);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


static void
cmd_mlib_search (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	GList *n = NULL;
	gchar *query;

	query = mlib_query_from_args(argc, argv);
	if (!query) {
		print_error ("Unable to generate query");
	}

	res = xmmsc_medialib_select (conn, query);
	xmmsc_result_wait (res);
	g_free (query);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		gint id;

		if (!xmmsc_result_get_dict_entry_int32 (res, "id", &id)) {
			print_error ("Broken resultset");
		}

		n = g_list_prepend (n, XINT_TO_POINTER (id));
		xmmsc_result_list_next (res);
	}
	n = g_list_reverse (n);
	format_pretty_list (conn, n);
	g_list_free (n);

	xmmsc_result_unref (res);
}


static void
cmd_mlib_query (xmmsc_connection_t *conn, gint argc, gchar **argv)
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

	while (xmmsc_result_list_valid (res)) {
		xmmsc_result_dict_foreach (res, print_hash, NULL);
		xmmsc_result_list_next (res);
	}

	xmmsc_result_unref (res);
}


static void
cmd_mlib_playlist_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	GList *n = NULL;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}
	
	res = xmmsc_medialib_playlist_list (conn, argv[3]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		guint id;

		if (!xmmsc_result_get_uint (res, &id)) {
			print_error ("Broken resultset");
		}

		n = g_list_prepend (n, XINT_TO_POINTER(id));
		xmmsc_result_list_next (res);
	}
	n = g_list_reverse (n);
	format_pretty_list (conn, n);
	g_list_free (n);

	xmmsc_result_unref (res);
}


static void
cmd_mlib_playlist_save (xmmsc_connection_t *conn, gint argc, gchar **argv)
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
cmd_mlib_playlist_load (xmmsc_connection_t *conn, gint argc, gchar **argv)
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
cmd_mlib_playlists_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_medialib_playlists_list (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		gchar *name;

		if (!xmmsc_result_get_string (res, &name)) {
			print_error ("Broken resultset");
		}

		/* Hide all lists that start with _ */
		if (name[0] != '_') {
			print_info ("%s", name);
		}
		xmmsc_result_list_next (res);
	}
	xmmsc_result_unref (res);
}


static void
cmd_mlib_playlist_import (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *url;

	if (argc < 5) {
		print_error ("Supply a playlist name and url");
	}

	url = format_url (argv[4]);
	if (!url) {
		print_error ("Invalid url");
	}

	res = xmmsc_medialib_playlist_import (conn, argv[3], url);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Playlist imported");
}


static void
cmd_mlib_playlist_remove (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}

	res = xmmsc_medialib_playlist_remove (conn, argv[3]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Playlist removed");
}


static void
cmd_mlib_playlist_export (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *file;
	gchar *mime;

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

	if (!xmmsc_result_get_string (res, &file)) {
		print_error ("Broken resultset!");
	}

	fwrite (file, strlen (file), 1, stdout);
	print_info ("Playlist exported");

	xmmsc_result_unref (res);
}


static void
cmd_mlib_addpath (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar rpath[PATH_MAX];

	if (argc < 4) {
		print_error ("Supply a path to add!");
	}

	if (!realpath (argv[3], rpath)) {
		return;
	}

	if (!g_file_test (rpath, G_FILE_TEST_IS_DIR)) {
		return;
	}

	res = xmmsc_medialib_path_import (conn, rpath);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

static void
cmd_mlib_remove (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	int i;
	int32_t entryid;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Supply an id to remove!");
	}

	for (i = 3; i < argc; i++) {
		entryid = atoi (argv[i]);
		print_info("Removing entry %i", entryid);
		res = xmmsc_medialib_remove_entry (conn, entryid);
		xmmsc_result_wait (res);
		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);
	}
}

static void
cmd_mlib_rehash (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	guint id = 0;

	if (argc < 4) {
		print_info ("Rehashing whole medialib!");
	} else {
		id = strtol (argv[3], NULL, 10);
	}

	res = xmmsc_medialib_rehash (conn, id);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}
