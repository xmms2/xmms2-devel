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

#include "cmd_mlib.h"
#include "common.h"


/**
 * Function prototypes
 */
static void cmd_mlib_set_str (xmmsc_connection_t *conn,
                              gint argc, gchar **argv);
static void cmd_mlib_set_int (xmmsc_connection_t *conn,
                              gint argc, gchar **argv);
static void cmd_mlib_rmprop  (xmmsc_connection_t *conn,
                              gint argc, gchar **argv);
static void cmd_mlib_add (xmmsc_connection_t *conn,
                          gint argc, gchar **argv);
static void cmd_mlib_loadall (xmmsc_connection_t *conn,
                              gint argc, gchar **argv);
static void cmd_mlib_searchadd (xmmsc_connection_t *conn,
                                gint argc, gchar **argv);
static void cmd_mlib_search (xmmsc_connection_t *conn,
                             gint argc, gchar **argv);
static void cmd_mlib_addpath (xmmsc_connection_t *conn,
                              gint argc, gchar **argv);
static void cmd_mlib_rehash (xmmsc_connection_t *conn,
                             gint argc, gchar **argv);
static void cmd_mlib_remove (xmmsc_connection_t *conn,
                             gint argc, gchar **argv);
static void cmd_mlib_addcover (xmmsc_connection_t *conn,
                               gint argc, gchar **arv);

cmds mlib_commands[] = {
	{ "add", "[url] - Add 'url' to medialib", cmd_mlib_add },
	{ "loadall", "Load everything from the mlib to the playlist", cmd_mlib_loadall },
	{ "searchadd", "[artist:Dismantled] ... - Search for, and add songs to playlist", cmd_mlib_searchadd },
	{ "search", "[artist:Dismantled] ... - Search for songs matching criteria", cmd_mlib_search },
	{ "addpath", "[path] - Import metadata from all media files under 'path'", cmd_mlib_addpath },
	{ "rehash", "Force the medialib to check whether its data is up to date", cmd_mlib_rehash },
	{ "remove", "Remove an entry from medialib", cmd_mlib_remove },
	{ "setstr", "[id, key, value, (source)] Set a string property together with a medialib entry.", cmd_mlib_set_str },
	{ "setint", "[id, key, value, (source)] Set a int property together with a medialib entry.", cmd_mlib_set_int },
	{ "rmprop", "[id, key, (source)] Remove a property from a medialib entry", cmd_mlib_rmprop },
	{ "addcover", "[file] [id] ... - Add a cover image on id(s).", cmd_mlib_addcover },
	{ NULL, NULL, NULL },
};


static void
cmd_mlib_help (void)
{
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
		cmd_mlib_help ();
		return;
	}

	for (i = 0; mlib_commands[i].name; i++) {
		if (g_ascii_strcasecmp (mlib_commands[i].name, argv[2]) == 0) {
			mlib_commands[i].func (conn, argc, argv);
			return;
		}
	}

	cmd_mlib_help ();
	print_error ("Unrecognised mlib command: %s", argv[2]);
}


void
cmd_info (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gint id;

	if (argc > 2) {
		gint cnt;

		for (cnt = 2; cnt < argc; cnt++) {
			id = strtol (argv[cnt], (gchar**) NULL, 10);

			res = xmmsc_medialib_get_info (conn, id);
			xmmsc_result_wait (res);
			val = xmmsc_result_get_value (res);

			if (xmmsv_get_error (val, &errmsg)) {
				print_error ("%s", errmsg);
			}

			xmmsv_dict_foreach (val, print_entry, val);
			xmmsc_result_unref (res);
		}

	} else {
		res = xmmsc_playback_current_id (conn);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);

		if (xmmsv_get_error (val, &errmsg)) {
			print_error ("%s", errmsg);
		}

		if (!xmmsv_get_int (val, &id)) {
			print_error ("Broken resultset");
		}
		xmmsc_result_unref (res);

		res = xmmsc_medialib_get_info (conn, id);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);

		if (xmmsv_get_error (val, &errmsg)) {
			print_error ("%s", errmsg);
		}

		xmmsv_dict_foreach (val, print_entry, val);
		xmmsc_result_unref (res);
	}
}

static void
cmd_mlib_set_str (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gint id;

	if (argc < 6) {
		print_error ("usage: setstr [id] [key] [value] ([source])");
	}

	id = strtol (argv[3], NULL, 10);

	if (argc == 7) {
		res = xmmsc_medialib_entry_property_set_str_with_source (conn,
		                                                         id,
		                                                         argv[6],
		                                                         argv[4],
		                                                         argv[5]);
	} else {
		res = xmmsc_medialib_entry_property_set_str (conn, id, argv[4],
		                                             argv[5]);
	}
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	xmmsc_result_unref (res);
}

static void
cmd_mlib_set_int (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gint id;

	if (argc < 6) {
		print_error ("usage: setint [id] [key] [value] ([source])");
	}

	id = strtol (argv[3], NULL, 10);

	if (argc == 7) {
		res = xmmsc_medialib_entry_property_set_int_with_source (conn,
		                                                         id,
		                                                         argv[6],
		                                                         argv[4],
		                                                         atoi (argv[5]));
	} else {
		res = xmmsc_medialib_entry_property_set_int (conn, id, argv[4],
		                                             atoi (argv[5]));
	}
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	xmmsc_result_unref (res);
}

static void
cmd_mlib_rmprop (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gint id;

	if (argc < 5) {
		print_error ("usage: rmprop [id] [key] ([source])");
	}

	id = strtol (argv[3], NULL, 10);

	if (argc == 6) {
		res = xmmsc_medialib_entry_property_remove_with_source (conn,
		                                                        id,
		                                                        argv[5],
		                                                        argv[4]);
	} else {
		res = xmmsc_medialib_entry_property_remove (conn,
		                                            id,
		                                            argv[4]);
	}

	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	xmmsc_result_unref (res);
}

static void
cmd_mlib_add (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gint i;

	for (i = 3; argv[i]; i++) {
		gchar *url;

		url = format_url (argv[i], G_FILE_TEST_IS_REGULAR);
		if (url) {
			res = xmmsc_medialib_add_entry (conn, url);
			xmmsc_result_wait (res);
			g_free (url);

			val = xmmsc_result_get_value (res);
			if (xmmsv_get_error (val, &errmsg)) {
				print_error ("%s", errmsg);
			}

			print_info ("Added %s to medialib", argv[i]);
			xmmsc_result_unref (res);
		}
	}
}


static void
cmd_mlib_loadall (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	xmmsv_coll_t *all = xmmsv_coll_universe ();
	xmmsv_t *empty = xmmsv_new_list ();

	/* Load in another playlist */
	if (argc == 4) {
		playlist = argv[3];
	}

	res = xmmsc_playlist_add_collection (conn, playlist, all, empty);
	xmmsc_result_wait (res);

	xmmsv_coll_unref (all);
	xmmsv_unref (empty);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
}


static void
cmd_mlib_searchadd (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	xmmsv_coll_t *query;
	gchar *pattern;
	gchar **args;
	int i;
	xmmsv_t *empty = xmmsv_new_list ();

	if (argc < 4) {
		print_error ("give a search pattern of the form "
		             "[field1:val1 [field2:val2 ...]]");
	}

	args = g_new0 (char*, argc - 2);
	for (i = 0; i < argc - 3; i++) {
		args[i] = string_escape (argv[i + 3]);
	}
	args[i] = NULL;

	pattern = g_strjoinv (" ", args);
	if (!xmmsv_coll_parse (pattern, &query)) {
		print_error ("Unable to generate query");
	}

	for (i = 0; i < argc - 3; i++) {
		g_free (args[i]);
	}
	g_free (args);
	g_free (pattern);

	/* FIXME: Always add to active playlist: allow loading in other playlist! */
	res = xmmsc_playlist_add_collection (conn, NULL, query, empty);
	xmmsc_result_wait (res);
	xmmsv_coll_unref (query);
	xmmsv_unref (empty);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
}

static void
cmd_mlib_search (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	xmmsv_list_iter_t *it;
	GList *n = NULL;
	xmmsv_coll_t *query;
	gchar *pattern;
	gchar **args;
	int i;

	if (argc < 4) {
		print_error ("give a search pattern of the form "
		             "[field1:val1 [field2:val2 ...]]");
	}

	args = g_new0 (char*, argc - 2);
	for (i = 0; i < argc - 3; i++) {
		args[i] = string_escape (argv[i + 3]);
	}
	args[i] = NULL;

	pattern = g_strjoinv (" ", args);
	if (!xmmsv_coll_parse (pattern, &query)) {
		print_error ("Unable to generate query");
	}

	for (i = 0; i < argc - 3; i++) {
		g_free (args[i]);
	}
	g_free (args);
	g_free (pattern);

	res = xmmsc_coll_query_ids (conn, query, NULL, 0, 0);
	xmmsc_result_wait (res);
	xmmsv_coll_unref (query);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	xmmsv_get_list_iter (val, &it);
	while (xmmsv_list_iter_valid (it)) {
		gint id;
		xmmsv_t *val_id;

		if (!xmmsv_list_iter_entry (it, &val_id) ||
		    !xmmsv_get_int (val_id, &id)) {
			print_error ("Broken resultset");
		}

		n = g_list_prepend (n, XINT_TO_POINTER (id));
		xmmsv_list_iter_next (it);
	}
	n = g_list_reverse (n);
	format_pretty_list (conn, n);
	g_list_free (n);

	xmmsc_result_unref (res);
}


static void
cmd_mlib_addpath (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gint i;

	if (argc < 4) {
		print_error ("Missing argument(s)");
	}

	for (i = 3; i < argc; i++) {
		gchar *rfile;

		rfile = format_url (argv[i], G_FILE_TEST_IS_DIR);
		if (!rfile) {
			print_info ("Ignoring invalid path '%s'", argv[i]);
			continue;
		}

		res = xmmsc_medialib_import_path (conn, rfile);
		g_free (rfile);

		xmmsc_result_wait (res);

		val = xmmsc_result_get_value (res);
		if (xmmsv_get_error (val, &errmsg)) {
			print_info ("Cannot add path '%s': %s", argv[i], errmsg);
		}

		xmmsc_result_unref (res);
	}
}

static void
cmd_mlib_remove (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	int i;
	int32_t entryid;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc < 4) {
		print_error ("Supply an id to remove!");
	}

	for (i = 3; i < argc; i++) {
		entryid = atoi (argv[i]);
		print_info ("Removing entry %i", entryid);
		res = xmmsc_medialib_remove_entry (conn, entryid);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);
		if (xmmsv_get_error (val, &errmsg)) {
			print_error ("%s", errmsg);
		}
		xmmsc_result_unref (res);
	}
}

static void
cmd_mlib_rehash (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	guint id = 0;

	if (argc < 4) {
		print_info ("Rehashing whole medialib!");
	} else {
		id = strtol (argv[3], NULL, 10);
	}

	res = xmmsc_medialib_rehash (conn, id);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
}

static void
cmd_mlib_addcover (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	const gchar *filename;
	GIOChannel* io = NULL;
	GError *error = NULL;

	if (argc < 5) {
		print_error ("Filename and at least one id needed!");
	}

	filename = argv[3];
	if (g_ascii_strcasecmp (filename, "-")) {
		io = g_io_channel_new_file (filename, "r", &error);
	} else {
		io = g_io_channel_unix_new (STDIN_FILENO);
	}

	if (io) {
		gchar *contents = NULL;
		gsize filesize = 0;
		const gchar *hash;
		gchar **id_arg;
		error = NULL;

		g_io_channel_set_encoding (io, NULL, &error);

		if (g_io_channel_read_to_end (io, &contents, &filesize, &error) == G_IO_STATUS_ERROR) {
			print_error ("Error reading file: %s", error->message);
		}

		if (g_io_channel_shutdown (io, FALSE, &error) == G_IO_STATUS_ERROR) {
			print_error ("Error closing file: %s", error->message);
		}

		res = xmmsc_bindata_add (conn, (guchar*)contents, filesize);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);

		g_free (contents);

		if (xmmsv_get_error (val, &errmsg)) {
			print_error ("%s", errmsg);
		}

		if (!xmmsv_get_string (val, &hash)) {
			print_error ("Could not extract hash from result!");
		}

		for (id_arg = argv + 4; id_arg < argv + argc; ++id_arg) {
			xmmsc_result_t *res2;
			xmmsv_t *val2;
			uint32_t id;
			gchar *end;

			id = strtoul (*id_arg, &end, 10);
			if (*id_arg == end) {
				print_info ("Could not convert '%s' to an unsigned integer", *id_arg);
				continue;
			}

			res2 = xmmsc_medialib_entry_property_set_str (conn, id, "picture_front", hash);
			xmmsc_result_wait (res2);

			val2 = xmmsc_result_get_value (res2);
			if (xmmsv_get_error (val2, &errmsg)) {
				print_info ("%s", errmsg);
			}
			xmmsc_result_unref (res2);
		}

		xmmsc_result_unref (res);

	} else {
		print_error ("Could not open file! (%s)", error->message);
	}
}
