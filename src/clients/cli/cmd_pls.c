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
#include "cmd_pls.h"
#include "common.h"

extern gchar *listformat;

static void
add_item_to_playlist (xmmsc_connection_t *conn, gchar *item)
{
	xmmsc_result_t *res;
	gchar *url;

	url = format_url (item, G_FILE_TEST_IS_REGULAR);
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

		url = format_url (argv[i], G_FILE_TEST_IS_REGULAR);
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
cmd_addarg (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *url;

	if (argc < 4) {
		print_error ("Need a filename and args to add");
	}

	url = format_url (argv[2], G_FILE_TEST_IS_REGULAR);
	if (!url) {
		print_error ("Invalid url");
	}

	res = xmmsc_playlist_add_args (conn, url, argc - 3,
	                               (const char **) &argv[3]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't add %s to playlist: %s\n", url,
		             xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Added %s", url);

	g_free (url);
}

void
cmd_insert (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	guint pos;
	gchar *url;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Need a position and a file");
	}

	pos = strtol (argv[2], NULL, 10);
	url = format_url (argv[3], G_FILE_TEST_IS_REGULAR);
	if (!url) {
		print_error ("Invalid url");
	}

	res = xmmsc_playlist_insert (conn, pos, url);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Unable to add %s at postion %u: %s", url,
		             pos, xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Inserted %s at %u", url, pos);

	g_free (url);
}

void
cmd_insertid (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	guint pos, mlib_id;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("Need a position and a medialib id");
	}

	pos = strtol (argv[2], NULL, 10);
	mlib_id = strtol (argv[3], NULL, 10);

	res = xmmsc_playlist_insert_id(conn, pos, mlib_id);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Unable to insert %u at position %u: %s", pos, 
		             mlib_id, xmmsc_result_get_error(res));
	}
	xmmsc_result_unref (res);

	print_info ("Inserted %u at position %u", mlib_id, pos);
}

void
cmd_radd (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gint i;

	if (argc < 3) {
		print_error ("Missing argument(s)");
	}

	for (i = 2; i < argc; i++) {
		gchar *rfile;

		rfile = format_url (argv[i], G_FILE_TEST_IS_DIR);
		if (!rfile) {
			print_info ("Ignoring invalid path '%s'", argv[i]);
			continue;
		}

		res = xmmsc_playlist_radd (conn, rfile);
		g_free (rfile);

		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_info ("Cannot add path '%s': %s",
			            argv[i], xmmsc_result_get_error (res));
		}

		xmmsc_result_unref (res);
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
	gchar *properties, *pbuf;
	gchar ***prop_tokens;
	gint i, j, prop_tokens_len, properties_len = 0;
	
	if (argc < 3) {
		print_error ("Sort needs a property to sort on, %d", argc);
	}

	prop_tokens_len = argc - 2;
	prop_tokens = g_new (gchar **, prop_tokens_len);
	for (i = 2; i < argc; i++) {
		guint temp_len;
		argv[i] = g_strchug (argv[i]);
		prop_tokens[i-2] = g_strsplit (argv[i], " ", 0);
		temp_len = g_strv_length (prop_tokens[i-2]);

		for (j = 0; j < temp_len; j++) {
			properties_len += strlen (prop_tokens[i-2][j]) + (i > 2 || j > 0 ? 1 : 0);
		}
	}

	properties = g_new (gchar, properties_len+1);
	pbuf = properties;
	for (i = 0; i < prop_tokens_len; i++) {
		guint temp_len = g_strv_length (prop_tokens[i]);
		for (j = 0; j < temp_len; j++ ) {
			size_t prop_len = strlen (prop_tokens[i][j]);
			if (!prop_len ) {
				continue;
			}
			if (i > 0 || j > 0) {
				*pbuf = ' ';
				pbuf++;
			}
			memcpy (pbuf, prop_tokens[i][j], prop_len);
			pbuf += prop_len;
		}
	}
	*pbuf = '\0';

	res = xmmsc_playlist_sort (conn, properties);
	xmmsc_result_wait (res);
	
	for (i = 0; i < prop_tokens_len; i++) {
		g_strfreev (prop_tokens[i]);
	}
	g_free (properties);

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
		print_error ("Remove needs a position to be removed");
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
		gint pos = sort[i];

		xmmsc_result_t *res = xmmsc_playlist_remove (conn, pos);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("Couldn't remove %d (%s)", pos,
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
		gchar line[80];
		gint playtime = 0;
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

		if (p == pos) {
			print_info ("->[%d/%d] %s", pos, ui, line);
		} else {
			print_info ("  [%d/%d] %s", pos, ui, line);
		}

		pos++;

		xmmsc_result_unref (info_res);
		xmmsc_result_list_next (res);
	}
	xmmsc_result_unref (res);

	/* rounding */
	total_playtime += 500;

	print_info ("\nTotal playtime: %d:%02d:%02d", total_playtime / 3600000, 
	            (total_playtime / 60000) % 60, (total_playtime / 1000) % 60);
}
