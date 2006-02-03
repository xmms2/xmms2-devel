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

#include "common.h"

gint
res_has_key (xmmsc_result_t *res, const gchar *key)
{
	return xmmsc_result_get_dict_entry_type (res, key) != XMMSC_RESULT_VALUE_TYPE_NONE;
}


gchar *
format_url (gchar *item)
{
	gchar *url, rpath[PATH_MAX], *p;

	p = strchr (item, ':');
	if (!(p && p[1] == '/' && p[2] == '/')) {
		/* OK, so this is NOT an valid URL */

		if (!realpath (item, rpath)) {
			return NULL;
		}

		if (!g_file_test (rpath, G_FILE_TEST_IS_REGULAR)) {
			return NULL;
		}

		url = g_strdup_printf ("file://%s", rpath);
	} else {
		url = g_strdup_printf ("%s", item);
	}

	return url;
}


void
print_info (const gchar *fmt, ...)
{
	gchar buf[8096];
	va_list ap;
	
	va_start (ap, fmt);
	g_vsnprintf (buf, 8096, fmt, ap);
	va_end (ap);

	g_print ("%s\n", buf);
}


void
print_error (const gchar *fmt, ...)
{
	gchar buf[1024];
	va_list ap;
	
	va_start (ap, fmt);
	g_vsnprintf (buf, 1024, fmt, ap);
	va_end (ap);

	g_print ("ERROR: %s\n", buf);

	exit (-1);
}


void
print_hash (const void *key, xmmsc_result_value_type_t type, 
			const void *value, void *udata)
{
	if (type == XMMSC_RESULT_VALUE_TYPE_STRING) {
		print_info ("%s = %s", key, value);
	} else {
		print_info ("%s = %d", key, XPOINTER_TO_INT (value));
	}
}


void
print_entry (const void *key, xmmsc_result_value_type_t type, 
			 const void *value, const gchar *source, void *udata)
{
	gchar *conv;
	gsize r, w;
	GError *err = NULL;

	if (type == XMMSC_RESULT_VALUE_TYPE_STRING) {
		conv = g_locale_from_utf8 (value, -1, &r, &w, &err);
		print_info ("[%s] %s = %s", source, key, conv);
		g_free (conv);
	} else {
		print_info ("[%s] %s = %d", source, key, XPOINTER_TO_INT (value));
	}
}


void
format_pretty_list (xmmsc_connection_t *conn, GList *list)
{
	guint count = 0;
	GList *n;
	
	print_info ("-[Result]-----------------------------------------------------------------------");
	print_info ("Id   | Artist            | Album                     | Title");

	for (n = list; n; n = g_list_next (n)) {
		gchar *title;
		xmmsc_result_t *res;
		gint mid = XPOINTER_TO_INT (n->data);

		if (mid) {
			res = xmmsc_medialib_get_info (conn, mid);
			xmmsc_result_wait (res);
		} else {
			print_error ("Empty result!");
		}

		if (xmmsc_result_get_dict_entry_str (res, "title", &title)) {
			gchar *artist, *album;
			if (!xmmsc_result_get_dict_entry_str (res, "artist", &artist)) {
				artist = "Unknown";
			}

			if (!xmmsc_result_get_dict_entry_str (res, "album", &album)) {
				album = "Unknown";
			}

			print_info ("%-5.5d| %-17.17s | %-25.25s | %-25.25s",
			            mid, artist, album, title);
		} else {
			gchar *url, *filename;
			xmmsc_result_get_dict_entry_str (res, "url", &url);
			if (url) {
				filename = g_path_get_basename (url);
				if (filename) {
					print_info ("%-5.5d| %s", mid, filename);
					g_free (filename);
				}
			}
		}
		count++;
		xmmsc_result_unref (res);
	}
	print_info ("-------------------------------------------------------------[Count:%6.d]-----", count);
}
