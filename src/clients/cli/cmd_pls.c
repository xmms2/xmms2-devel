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
#include "cmd_pls.h"
#include "common.h"

extern gchar *listformat;

static void
add_item_to_playlist (xmmsc_connection_t *conn, gchar *item)
{
	xmmsc_result_t *res;
	gchar *url;

	url = format_url (item);
	if (!url) {
		print_error ("Invalid url");
	}

	res = xmmsc_playlist_add (conn, url);
	xmmsc_result_wait (res);
	g_free (url);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't add %s to playlist: %s\n", item,
		             xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Added %s", item);
}


static void
add_directory_to_playlist (xmmsc_connection_t *conn, gchar *directory,
                           gboolean recursive)
{
	GSList *entries = NULL;
	const gchar *entry;
	gchar *buf;
	GDir *dir;

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		print_error ("cannot open directory: %s", directory);
	}

	while ((entry = g_dir_read_name (dir))) {
		entries = g_slist_prepend (entries, g_strdup (entry));
	}
	g_dir_close (dir);

	/* g_dir_read_name() will return the entries in a undefined
	 * order, so sort the list now.
	 */
	entries = g_slist_sort (entries, (GCompareFunc) strcmp);

	while (entries) {
		buf = g_build_path (G_DIR_SEPARATOR_S, directory, 
		                    entries->data, NULL);

		if (g_file_test (buf, G_FILE_TEST_IS_DIR)) {
			if (recursive) {
				add_directory_to_playlist (conn, buf, recursive);
			}
		} else {
			add_item_to_playlist (conn, buf);
		}

		g_free (buf);
		g_free (entries->data);
		entries = g_slist_delete_link (entries, entries);
	}
}


void
cmd_addid (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;
	xmmsc_result_t *res;

	if (argc < 3) {
		print_error ("Need a medialib id to add");
	}

	for (i = 2; argv[i]; i++) {
		guint id = strtoul (argv[i], NULL, 10);
		if (id) {
			res = xmmsc_playlist_add_id (conn, id);
			xmmsc_result_wait (res);

			if (xmmsc_result_iserror (res)) {
				print_error ("Couldn't add %d to playlist: %s", id, 
							 xmmsc_result_get_error (res));
			}
			xmmsc_result_unref (res);

			print_info ("Added medialib id %d to playlist", id);
		}
	}
}


void
cmd_addpls (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3) {
		print_error ("Need a playlist url to add");
	}

	for (i = 2; argv[i]; i++) {
		xmmsc_result_t *res;
		gchar *url;

		url = format_url (argv[i]);
		if (!url) {
			print_error ("Invalid url");
		}

		res = xmmsc_medialib_playlist_import (conn, "_xmms2cli", url);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);

		res = xmmsc_medialib_playlist_load (conn, "_xmms2cli");
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("%s", xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);

		print_info ("Added playlist %s", url);
		g_free (url);
	}
}


void
cmd_add (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3) {
		print_error ("Need a filename to add");
	}

	for (i = 2; argv[i]; i++) {
		add_item_to_playlist (conn, argv[i]);
	}
}


void
cmd_radd (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3) {
		print_error ("Need a directory to add");
	}

	for (i = 2; argv[i]; i++) {
		if (!g_file_test (argv[i], G_FILE_TEST_IS_DIR)) {
			print_info ("not a directory: %s", argv[i]);
		} else {
			add_directory_to_playlist (conn, argv[i], TRUE);
		}
	}
}


void
cmd_clear (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_playlist_clear (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


void
cmd_shuffle (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	
	res = xmmsc_playlist_shuffle (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}


void
cmd_sort (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	
	if (argc != 3) {
		print_error ("Sort needs a property to sort on, %d", argc);
	}
	
	res = xmmsc_playlist_sort (conn, argv[2]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
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
	gint i, size = 0;
	gint *sort;

	if (argc < 3) {
		print_error ("Remove needs a ID to be removed");
	}

	sort = g_malloc (sizeof (gint) * argc);

	for (i = 2; i < argc; i++) {
		gchar *endptr = NULL;
		sort[size] = strtol (argv[i], &endptr, 10);
		if (endptr != argv[i]) {
			size++;
		}
	}

	qsort (sort, size, sizeof (gint), &cmp);

	for (i = 0; i < size; i++) {
		gint id = sort[i];

		xmmsc_result_t *res = xmmsc_playlist_remove (conn, id);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("Couldn't remove %d (%s)", id,
			             xmmsc_result_get_error (res));
		}
		xmmsc_result_unref (res);
	}
	
	g_free (sort);
}


void
cmd_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gulong total_playtime = 0;
	guint p = 0;
	guint pos = 0;
	gsize r, w;

	res = xmmsc_playlist_current_pos (conn);
	xmmsc_result_wait (res);

	if (!xmmsc_result_iserror (res)) {
		if (!xmmsc_result_get_uint (res, &p)) {
			print_error ("Broken resultset");
		}
		xmmsc_result_unref (res);
	}

	res = xmmsc_playlist_list (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		xmmsc_result_t *info_res;
		GError *err = NULL;
		gchar line[80];
		gint playtime = 0;
		gchar *conv;
		guint ui;

		if (!xmmsc_result_get_uint (res, &ui)) {
			print_error ("Broken resultset");
		}

		info_res = xmmsc_medialib_get_info (conn, ui);
		xmmsc_result_wait (info_res);

		if (xmmsc_result_iserror (info_res)) {
			print_error ("%s", xmmsc_result_get_error (info_res));
		}

		if (xmmsc_result_get_dict_entry_int32 (info_res, "duration", &playtime)) {
			total_playtime += playtime;
		}
		
		if (res_has_key (info_res, "channel")) {
			if (res_has_key (info_res, "title")) {
				xmmsc_entry_format (line, sizeof (line),
				                    "[stream] ${title}", info_res);
			} else {
				xmmsc_entry_format (line, sizeof (line),
				                    "${channel}", info_res);
			}
		} else if (!res_has_key (info_res, "title")) {
			gchar *url, *filename;
		  	gchar dur[10];
			
			xmmsc_entry_format (dur, sizeof (dur),
			                    "(${minutes}:${seconds})", info_res);
			
			if (xmmsc_result_get_dict_entry_str (info_res, "url", &url)) {
				filename = g_path_get_basename (url);
				if (filename) {
					g_snprintf (line, sizeof (line), "%s %s", filename, dur);
					g_free (filename);
				} else {
					g_snprintf (line, sizeof (line), "%s %s", url, dur);
				}
			}
		} else {
			xmmsc_entry_format (line, sizeof(line), listformat, info_res);
		}


		conv = g_locale_from_utf8 (line, -1, &r, &w, &err);

		if (p == pos) {
			print_info ("->[%d/%d] %s", pos, ui, conv);
		} else {
			print_info ("  [%d/%d] %s", pos, ui, conv);
		}
		g_free (conv);
		pos++;

		if (err) {
			print_info ("convert error %s", err->message);
		}
		g_clear_error (&err);

		xmmsc_result_unref (info_res);
		xmmsc_result_list_next (res);
	}
	xmmsc_result_unref (res);

	print_info ("\nTotal playtime: %d:%02d:%02d", total_playtime / 3600000, 
	            (total_playtime / 60000) % 60, (total_playtime / 1000) % 60);
}
