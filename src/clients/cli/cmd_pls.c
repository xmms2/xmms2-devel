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
#include "cmd_pls.h"
#include "common.h"

cmds plist_commands[] = {
	{ "list", "List all available playlists", cmd_playlists_list },
	{ "active", "Displays the name of the active playlist", cmd_playlist_active },
	{ "create", "[playlistname] - Create a playlist", cmd_playlist_create },
	{ "type", "[playlistname] [type] - Set the type of the playlist (list, queue, pshuffle)", cmd_playlist_type },
	{ "load", "[playlistname] - Load 'playlistname' stored in medialib", cmd_playlist_load },
	{ "remove", "[playlistname] - Remove a playlist", cmd_playlist_remove },
	{ NULL, NULL, NULL },
};


extern gchar *listformat;

static void
add_item_to_playlist (xmmsc_connection_t *conn, gchar *playlist, gchar *item)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	gchar *url;
	const char *errmsg;

	url = format_url (item, G_FILE_TEST_IS_REGULAR);
	if (!url) {
		print_error ("Invalid url");
	}

	res = xmmsc_playlist_add_url (conn, playlist, url);
	xmmsc_result_wait (res);
	g_free (url);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't add %s to playlist: %s\n", item, errmsg);
	}
	xmmsc_result_unref (res);

	print_info ("Added %s", item);
}


static const gchar *
get_playlist_type_string (xmmsv_coll_type_t type)
{
	switch (type) {
	case XMMS_COLLECTION_TYPE_IDLIST:        return "list";
	case XMMS_COLLECTION_TYPE_QUEUE:         return "queue";
	case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:  return "pshuffle";
	default:                                 return "unknown";
	}
}

static void
coll_copy_attributes (const char *key, const char *value, void *udata)
{
	xmmsv_coll_t *coll = (xmmsv_coll_t*) udata;

	xmmsv_coll_attribute_set (coll, key, value);
}

static void
playlist_setup_pshuffle (xmmsc_connection_t *conn, xmmsv_coll_t *coll, gchar *ref)
{
	xmmsc_result_t *psres;
	xmmsv_t *psval;
	const char *errmsg;
	xmmsv_coll_t *refcoll;
	gchar *s_name, *s_namespace;

	if (!coll_read_collname (ref, &s_name, &s_namespace)) {
		print_error ("invalid source collection name");
	}

	/* Quick shortcut to use Universe for "All Media" */
	if (strcmp (s_name, "All Media") == 0) {
		refcoll = xmmsv_coll_universe ();
	} else {
		psres = xmmsc_coll_get (conn, s_name, s_namespace);
		xmmsc_result_wait (psres);

		psval = xmmsc_result_get_value (psres);
		if (xmmsv_get_error (psval, &errmsg)) {
			print_error ("%s", errmsg);
		}

		refcoll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_REFERENCE);
		xmmsv_coll_attribute_set (refcoll, "reference", s_name);
		xmmsv_coll_attribute_set (refcoll, "namespace", s_namespace);
	}

	/* Set operand */
	xmmsv_coll_add_operand (coll, refcoll);
	xmmsv_coll_unref (refcoll);

	g_free (s_name);
	g_free (s_namespace);
}


static void
cmd_playlist_help (void)
{
	gint i;

	print_info ("Available playlist commands:");
	for (i = 0; plist_commands[i].name; i++) {
		print_info ("  %s\t %s", plist_commands[i].name,
		            plist_commands[i].help);
	}
}


void
cmd_playlist (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;
	if (argc < 3) {
		cmd_playlist_help ();
		return;
	}

	for (i = 0; plist_commands[i].name; i++) {
		if (g_ascii_strcasecmp (plist_commands[i].name, argv[2]) == 0) {
			plist_commands[i].func (conn, argc, argv);
			return;
		}
	}

	cmd_playlist_help ();
	print_error ("Unrecognised playlist command: %s", argv[2]);
}


void
cmd_addid (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;
	gchar *playlist = NULL;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc < 3) {
		print_error ("Need a medialib id to add");
	}

	for (i = 2; argv[i]; i++) {
		guint id = strtoul (argv[i], NULL, 10);
		if (id) {
			res = xmmsc_playlist_add_id (conn, playlist, id);
			xmmsc_result_wait (res);

			val = xmmsc_result_get_value (res);
			if (xmmsv_get_error (val, &errmsg)) {
				print_error ("Couldn't add %d to playlist: %s", id, errmsg);
			}
			xmmsc_result_unref (res);

			print_info ("Added medialib id %d to playlist", id);
		} else if (i == 2) {
			/* First argument is the playlist name */
			playlist = argv[i];
		}
	}
}

void
cmd_add (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	gint i;

	if (argc < 3) {
		print_error ("Need a filename to add");
	}

	for (i = 2; argv[i]; i++) {
		/* FIXME: Fulhack to check for optional playlist argument */
		if (i == 2 && argc > 3 && !g_file_test (argv[i], G_FILE_TEST_EXISTS)) {
			playlist = argv[i++];
		}

		add_item_to_playlist (conn, playlist, argv[i]);
	}
}

static xmmsv_t *
args_to_dict (gchar **argv, gint argc)
{
	xmmsv_t *dict;
	gint i;

	dict = xmmsv_new_dict ();
	if (!dict) {
		return NULL;
	}

	for (i = 0; i < argc; i++) {
		gchar **tuple;
		xmmsv_t *val;

		/* Split the string into arg and value. */
		tuple = g_strsplit (argv[i], "=", 2);
		if (tuple[0] && tuple[1]) {
			val = xmmsv_new_string (tuple[1]);
		} else if (tuple[0]) {
			val = xmmsv_new_none ();
		} else {
			g_strfreev (tuple);
			continue; /* Empty tuple, means empty string. */
		}
		if (!val) {
			xmmsv_unref (dict);
			return NULL;
		}

		xmmsv_dict_set (dict, tuple[0], val);

		g_strfreev (tuple);
		xmmsv_unref (val);
	}

	return dict;
}

void
cmd_addarg (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gchar *playlist = NULL;
	gchar *url;
	int arg_start = 3;
	xmmsv_t *args;

	if (argc < 4) {
		print_error ("Need a filename and args to add");
	}

	url = format_url (argv[2], G_FILE_TEST_IS_REGULAR);
	if (!url) {
		url = format_url (argv[3], G_FILE_TEST_IS_REGULAR);
		if (!url) {
			print_error ("Invalid url");
		} else {
			/* FIXME: Fulhack to check for optional playlist argument */
			playlist = argv[2];
			arg_start = 4;
		}
	}

	args = args_to_dict (&argv[arg_start], argc - arg_start);
	res = xmmsc_playlist_add_full (conn, playlist, url, args);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't add %s to playlist: %s\n", url, errmsg);
	}
	xmmsc_result_unref (res);

	print_info ("Added %s", url);

	g_free (url);
}

void
cmd_insert (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	guint pos;
	gchar *url;
	char *endptr;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc < 4) {
		print_error ("Need a position and a file");
	}

	pos = strtol (argv[2], &endptr, 10);
	if (*endptr == '\0') {
		url = format_url (argv[3], G_FILE_TEST_IS_REGULAR);  /* No playlist name */
	} else {
		playlist = argv[2];  /* extract playlist name */
		pos = strtol (argv[3], NULL, 10);
		url = format_url (argv[4], G_FILE_TEST_IS_REGULAR);
	}

	if (!url) {
		print_error ("Invalid url");
	}

	res = xmmsc_playlist_insert_url (conn, playlist, pos, url);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Unable to add %s at postion %u: %s", url, pos, errmsg);
	}
	xmmsc_result_unref (res);

	print_info ("Inserted %s at %u", url, pos);

	g_free (url);
}

void
cmd_insertid (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	guint pos, mlib_id;
	char *endptr;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc < 4) {
		print_error ("Need a position and a medialib id");
	}

	pos = strtol (argv[2], &endptr, 10);
	if (*endptr == '\0') {
		mlib_id = strtol (argv[3], NULL, 10); /* No playlist name */
	} else {
		playlist = argv[2];  /* extract playlist name */
		pos = strtol (argv[3], NULL, 10);
		mlib_id = strtol (argv[4], NULL, 10);
	}

	res = xmmsc_playlist_insert_id (conn, playlist, pos, mlib_id);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Unable to insert %u at position %u: %s", mlib_id,
		             pos, errmsg);
	}

	print_info ("Inserted %u at position %u", mlib_id, pos);

	xmmsc_result_unref (res);
}

void
cmd_radd (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	xmmsc_result_t *res;
	gint i;

	if (argc < 3) {
		print_error ("Missing argument(s)");
	}

	for (i = 2; i < argc; i++) {
		gchar *rfile;
		xmmsv_t *val;
		const char *errmsg;

		if (i == 2 && argc > 3 && !g_file_test (argv[i], G_FILE_TEST_EXISTS)) {
			playlist = argv[i];
			continue;
		}

		rfile = format_url (argv[i], G_FILE_TEST_IS_DIR);
		if (!rfile) {
			print_info ("Ignoring invalid path '%s'", argv[i]);
			continue;
		}

		res = xmmsc_playlist_radd (conn, playlist, rfile);
		g_free (rfile);

		xmmsc_result_wait (res);

		val = xmmsc_result_get_value (res);
		if (xmmsv_get_error (val, &errmsg)) {
			print_info ("Cannot add path '%s': %s", argv[i], errmsg);
		}

		xmmsc_result_unref (res);
	}
}

void
cmd_rinsert (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist;
	gchar *endptr;
	xmmsc_result_t *res;
	guint pos;
	gint i, fileargn;

	if (argc < 4) {
		print_error ("Missing argument(s)");
	}


	pos = strtol (argv[2], &endptr, 10);
	if (*endptr == '\0') {
		playlist = NULL; /* No playlist name */
		fileargn = 3;
	} else {
		playlist = argv[2];  /* extract playlist name */
		pos = strtol (argv[3], NULL, 10);
		fileargn = 4;
	}

	for (i = fileargn; i < argc; i++) {
		gchar *rfile;
		xmmsv_t *val;
		const char *errmsg;

		rfile = format_url (argv[i], G_FILE_TEST_IS_DIR);
		if (!rfile) {
			print_info ("Ignoring invalid path '%s'", argv[i]);
			continue;
		}

		res = xmmsc_playlist_rinsert (conn, playlist, pos, rfile);
		g_free (rfile);

		xmmsc_result_wait (res);

		val = xmmsc_result_get_value (res);
		if (xmmsv_get_error (val, &errmsg)) {
			print_info ("Cannot insert path '%s' at position %u: %s",
			            argv[i], pos, errmsg);
		}

		xmmsc_result_unref (res);
	}
}

void
cmd_clear (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc == 3) {
		playlist = argv[2];
	}

	res = xmmsc_playlist_clear (conn, playlist);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
}


void
cmd_shuffle (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc == 3) {
		playlist = argv[2];
	}

	res = xmmsc_playlist_shuffle (conn, playlist);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
}


void
cmd_sort (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist;
	xmmsv_t *sortby;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc < 3) {
		print_error ("Sort needs a property to sort on");
	} else if (argc == 3) {
		playlist = NULL;
		sortby = xmmsv_make_stringlist (&argv[2], argc - 2);
	} else {
		playlist = argv[2];
		sortby = xmmsv_make_stringlist (&argv[3], argc - 3);
	}

	res = xmmsc_playlist_sort (conn, playlist, sortby);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
	xmmsv_unref (sortby);
}


gint
cmp (const void *av, const void *bv)
{
	gint result;
	gint a = *(gint *) av;
	gint b = *(gint *) bv;

	result = (a > b ? -1 : 1);

	if (a == b) {
		result = 0;
	}

	return result;
}


void
cmd_remove (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	gint i, size = 0;
	gint *sort;

	if (argc < 3) {
		print_error ("Remove needs a position to be removed");
	}

	sort = g_malloc (sizeof (gint) * argc);

	for (i = 2; i < argc; i++) {
		gchar *endptr = NULL;
		sort[size] = strtol (argv[i], &endptr, 10);
		if (endptr != argv[i]) {
			size++;
		} else if (i == 2) {
			/* First argument is the playlist name */
			playlist = argv[i];
		}
	}

	qsort (sort, size, sizeof (gint), &cmp);

	for (i = 0; i < size; i++) {
		gint pos = sort[i];
		xmmsv_t *val;
		const char *errmsg;

		xmmsc_result_t *res = xmmsc_playlist_remove_entry (conn, playlist, pos);
		xmmsc_result_wait (res);

		val = xmmsc_result_get_value (res);
		if (xmmsv_get_error (val, &errmsg)) {
			print_error ("Couldn't remove %d (%s)", pos, errmsg);
		}
		xmmsc_result_unref (res);
	}

	g_free (sort);
}


void
cmd_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist = NULL;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	xmmsv_list_iter_t *it;
	gulong total_playtime = 0;
	gint p = 0;
	guint pos = 0;

	if (argc > 2) {
		playlist = argv[2];
	}

	res = xmmsc_playlist_current_pos (conn, playlist);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (!xmmsv_is_error (val)) {
		if (!xmmsv_dict_entry_get_int (val, "position", &p)) {
			print_error ("Broken resultset");
		}
		xmmsc_result_unref (res);
	}

	res = xmmsc_playlist_list_entries (conn, playlist);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	xmmsv_get_list_iter (val, &it);
	while (xmmsv_list_iter_valid (it)) {
		xmmsc_result_t *info_res;
		xmmsv_t *val_id, *propdict, *info_val;
		const char *errmsg;
		gchar line[80];
		gint playtime = 0;
		gint ui;

		xmmsv_list_iter_entry (it, &val_id);
		if (!xmmsv_get_int (val_id, &ui)) {
			print_error ("Broken resultset");
		}

		info_res = xmmsc_medialib_get_info (conn, ui);
		xmmsc_result_wait (info_res);
		propdict = xmmsc_result_get_value (info_res);
		info_val = xmmsv_propdict_to_dict (propdict, NULL);

		if (xmmsv_get_error (info_val, &errmsg)) {
			print_error ("%s", errmsg);
		}

		if (xmmsv_dict_entry_get_int (info_val, "duration", &playtime)) {
			total_playtime += playtime;
		}

		if (xmmsv_dict_has_key (info_val, "channel")) {
			if (xmmsv_dict_has_key (info_val, "title")) {
				xmmsv_dict_format (line, sizeof (line),
				                   "[stream] ${title}", info_val);
			} else {
				xmmsv_dict_format (line, sizeof (line),
				                   "${channel}", info_val);
			}
		} else if (!xmmsv_dict_has_key (info_val, "title")) {
			const gchar *url;
			gchar dur[10];

			xmmsv_dict_format (dur, sizeof (dur),
			                   "(${minutes}:${seconds})", info_val);

			if (xmmsv_dict_entry_get_string (info_val, "url", &url)) {
				gchar *filename = g_path_get_basename (url);
				if (filename) {
					g_snprintf (line, sizeof (line), "%s %s", filename, dur);
					g_free (filename);
				} else {
					g_snprintf (line, sizeof (line), "%s %s", url, dur);
				}
			}
		} else {
			xmmsv_dict_format (line, sizeof (line), listformat, info_val);
		}

		if (p == pos) {
			print_info ("->[%d/%d] %s", pos, ui, line);
		} else {
			print_info ("  [%d/%d] %s", pos, ui, line);
		}

		pos++;

		xmmsc_result_unref (info_res);
		xmmsv_unref (info_val);
		xmmsv_list_iter_next (it);
	}
	xmmsc_result_unref (res);

	/* rounding */
	total_playtime += 500;

	print_info ("\nTotal playtime: %d:%02d:%02d", total_playtime / 3600000,
	            (total_playtime / 60000) % 60, (total_playtime / 1000) % 60);
}


void
cmd_move (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	guint cur_pos, new_pos, arg_start;
	gchar *playlist;

	if (argc < 4) {
		print_error ("You'll need to specifiy current and new position");
	}

	if (argc == 4) {
		playlist = NULL;
		arg_start = 2;
	} else {
		playlist = argv[2];
		arg_start = 3;
	}

	cur_pos = strtol (argv[arg_start], NULL, 10);
	new_pos = strtol (argv[arg_start + 1], NULL, 10);

	res = xmmsc_playlist_move_entry (conn, playlist, cur_pos, new_pos);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Unable to move playlist entry: %s", errmsg);
	}
	xmmsc_result_unref (res);

	print_info ("Moved %u to %u", cur_pos, new_pos);
}


void
cmd_playlist_load (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}

	res = xmmsc_playlist_load (conn, argv[3]);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
}


void
cmd_playlist_create (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gchar *playlist_name;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}

	playlist_name = argv[3];
	res = xmmsc_playlist_create (conn, playlist_name);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);
}


void
cmd_playlist_type (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *name;
	xmmsv_coll_type_t prevtype, newtype;
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	xmmsv_coll_t *coll;

	/* Read playlist name */
	if (argc < 4) {
		print_error ("usage: type_playlist [playlistname] [type] [options]");
	}
	name = argv[3];

	/* Retrieve the playlist operator */
	res = xmmsc_coll_get (conn, name, XMMS_COLLECTION_NS_PLAYLISTS);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	xmmsv_get_coll (val, &coll);
	prevtype = xmmsv_coll_get_type (coll);

	/* No type argument, simply display the current type */
	if (argc < 5) {
		print_info (get_playlist_type_string (prevtype));

	/* Type argument, set the new type */
	} else {
		gint typelen;
		gint idlistsize;
		xmmsc_result_t *saveres;
		xmmsv_t *saveval;
		xmmsv_coll_t *newcoll;
		gint i;

		typelen = strlen (argv[4]);
		if (g_ascii_strncasecmp (argv[4], "list", typelen) == 0) {
			newtype = XMMS_COLLECTION_TYPE_IDLIST;
		} else if (g_ascii_strncasecmp (argv[4], "queue", typelen) == 0) {
			newtype = XMMS_COLLECTION_TYPE_QUEUE;
		} else if (g_ascii_strncasecmp (argv[4], "pshuffle", typelen) == 0) {
			newtype = XMMS_COLLECTION_TYPE_PARTYSHUFFLE;

			/* Setup operand for party shuffle (set operand) ! */
			if (argc < 6) {
				print_error ("Give the source collection for the party shuffle");
			}

		} else {
			print_error ("Invalid playlist type (valid types: list, queue, pshuffle)");
		}

		/* Copy collection idlist, attributes and operand (if needed) */
		newcoll = xmmsv_coll_new (newtype);

		idlistsize = xmmsv_coll_idlist_get_size (coll);
		for (i = 0; i < idlistsize; i++) {
			guint id;
			xmmsv_coll_idlist_get_index (coll, i, &id);
			xmmsv_coll_idlist_append (newcoll, id);
		}

		xmmsv_coll_attribute_foreach (coll, coll_copy_attributes, newcoll);

		if (newtype == XMMS_COLLECTION_TYPE_PARTYSHUFFLE) {
			playlist_setup_pshuffle (conn, newcoll, argv[5]);
		}

		/* Overwrite with new collection */
		saveres = xmmsc_coll_save (conn, newcoll, name, XMMS_COLLECTION_NS_PLAYLISTS);
		xmmsc_result_wait (saveres);

		saveval = xmmsc_result_get_value (saveres);
		if (xmmsv_get_error (saveval, &errmsg)) {
			print_error ("Couldn't save %s : %s", name, errmsg);
		}

		xmmsv_coll_unref (newcoll);
		xmmsc_result_unref (saveres);
	}

	xmmsc_result_unref (res);
}


void
cmd_playlists_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	const gchar *active_name;
	xmmsc_result_t *res, *active_res;
	xmmsv_t *val, *active_val;
	const char *errmsg;
	xmmsv_list_iter_t *it;

	active_res = xmmsc_playlist_current_active (conn);
	xmmsc_result_wait (active_res);
	active_val = xmmsc_result_get_value (active_res);

	if (xmmsv_is_error (active_val) ||
	    !xmmsv_get_string (active_val, &active_name)) {
		active_name = NULL;
	}

	res = xmmsc_playlist_list (conn);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	xmmsv_get_list_iter (val, &it);
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_t *valstr;
		const gchar *name;

		xmmsv_list_iter_entry (it, &valstr);
		if (!xmmsv_get_string (valstr, &name)) {
			print_error ("Broken resultset");
		}

		/* Hide all lists that start with _ */
		if (name[0] != '_') {
			if (active_name != NULL && strcmp (active_name, name) == 0) {
				print_info ("->%s", name);
			} else {
				print_info ("  %s", name);
			}
		}
		xmmsv_list_iter_next (it);
	}
	xmmsc_result_unref (res);
	xmmsc_result_unref (active_res);
}

void
cmd_playlist_active (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	const gchar *active_name;
	xmmsc_result_t *active_res;
	xmmsv_t *active_val;

	active_res = xmmsc_playlist_current_active (conn);
	xmmsc_result_wait (active_res);
	active_val = xmmsc_result_get_value (active_res);

	if (!xmmsv_is_error (active_val) &&
	    xmmsv_get_string (active_val, &active_name)) {
		print_info ("%s",active_name);
	}

	xmmsc_result_unref (active_res);
}

void
cmd_playlist_remove (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	if (argc < 4) {
		print_error ("Supply a playlist name");
	}

	res = xmmsc_playlist_remove (conn, argv[3]);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}
	xmmsc_result_unref (res);

	print_info ("Playlist removed");
}

void
cmd_addpls (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *playlist;
	xmmsc_result_t *res, *res2;
	xmmsv_t *val;
	xmmsv_t *val2;
	const char *errmsg;
	xmmsv_coll_t *coll;
	gchar *url;

	if (argc < 3) {
		print_error ("Supply path to playlist file");
	}

	if (argc == 3) {
		playlist = NULL;
		url = format_url (argv[2], G_FILE_TEST_IS_REGULAR);
	} else {
		playlist = argv[2];
		url = format_url (argv[3], G_FILE_TEST_IS_REGULAR);
	}

	res = xmmsc_coll_idlist_from_playlist_file (conn, url);
	g_free (url);

	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	if (!xmmsv_get_coll (val, &coll)) {
		print_error ("Couldn't get collection from result!");
	}

	res2 = xmmsc_playlist_add_idlist (conn, playlist, coll);
	xmmsc_result_wait (res2);
	val2 = xmmsc_result_get_value (res2);
	if (xmmsv_get_error (val2, &errmsg)) {
		print_error ("%s", errmsg);
	}

	print_info ("Playlist with %d entries added", xmmsv_coll_idlist_get_size (coll));

	xmmsc_result_unref (res);
	xmmsc_result_unref (res2);
}
