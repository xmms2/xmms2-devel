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

#include "common.h"


gchar *
format_url (gchar *item, GFileTest test)
{
	gchar *url, rpath[PATH_MAX], *p;

	p = strchr (item, ':');
	if (!(p && p[1] == '/' && p[2] == '/')) {
		/* OK, so this is NOT an valid URL */

		if (!x_realpath (item, rpath)) {
			return NULL;
		}

		if (!g_file_test (rpath, test)) {
			return NULL;
		}

		url = g_strdup_printf ("file://%s", rpath);
	} else {
		url = g_strdup (item);
	}

	return x_path2url (url);
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

	g_printerr ("ERROR: %s\n", buf);

	exit (EXIT_FAILURE);
}


void
print_hash (const gchar *key, xmmsv_t *value, void *udata)
{
	xmmsv_type_t value_type;
	const char *string_val;
	unsigned int uint_val;
	int int_val;

	value_type = xmmsv_get_type (value);

	switch (value_type) {
		case XMMSV_TYPE_STRING:
			xmmsv_get_string (value, &string_val);
			print_info ("%s = %s", key, string_val);

			break;
		case XMMSV_TYPE_INT32:
			xmmsv_get_int (value, &int_val);
			print_info ("%s = %d", key, int_val);

			break;
		default:
			print_error ("unhandled hash value %i", value_type);
	}
}


static void
print_entry_string (xmmsv_t *v, const gchar *key, const gchar *source)
{
	const gchar *value;

	xmmsv_get_string (v, &value);

	/* Ok it's a string, if it's the URL property from the
	 * server source we need to decode it since it's
	 * encoded in the server
	 */
	if (strcmp (key, "url") == 0 && strcmp (source, "server") == 0) {
		/* First decode the URL encoding */
		xmmsv_t *tmp;
		gchar *url = NULL;
		const unsigned char *burl;
		unsigned int blen;

		tmp = xmmsv_decode_url (v);
		if (tmp && xmmsv_get_bin (tmp, &burl, &blen)) {
			url = g_malloc (blen + 1);
			memcpy (url, burl, blen);
			url[blen] = 0;
			xmmsv_unref (tmp);
		}

		/* Let's see if the result is valid utf-8. This must be done
		 * since we don't know the charset of the binary string */
		if (url && g_utf8_validate (url, -1, NULL)) {
			/* If it's valid utf-8 we don't have any problem just
			 * printing it to the screen
			 */
			print_info ("[%s] %s = %s", source, key, url);
		} else if (url) {
			/* Not valid utf-8 :-( We make a valid guess here that
			 * the string when it was encoded with URL it was in the
			 * same charset as we have on the terminal now.
			 *
			 * THIS MIGHT BE WRONG since different clients can have
			 * different charsets and DIFFERENT computers most likely
			 * have it.
			 */
			gchar *tmp2 = g_locale_to_utf8 (url, -1, NULL, NULL, NULL);
			/* Lets add a disclaimer */
			print_info ("[%s] %s = %s (charset guessed)", source, key, tmp2);
			g_free (tmp2);
		} else {
			/* Decoding the URL failed for some reason. That's not good. */
			print_info ("[%s] %s = (invalid encoding)", source, key);
		}

		g_free (url);
	} else {
		/* Normal strings is ALWAYS utf-8 no problem */
		print_info ("[%s] %s = %s", source, key, value);
	}
}

/* dumps a recursive key-source-val dict */
void
print_entry (const gchar *key, xmmsv_t *dict, void *udata)
{
	xmmsv_t *v;
	const gchar *source;
	if (xmmsv_get_type (dict) == XMMSV_TYPE_DICT) {
		xmmsv_dict_iter_t *it;
		xmmsv_get_dict_iter (dict, &it);

		while (xmmsv_dict_iter_valid (it)) {
			xmmsv_dict_iter_pair (it, &source, &v);
			switch (xmmsv_get_type (v)) {
			case XMMSV_TYPE_STRING:
				print_entry_string (v, key, source);
				break;
			case XMMSV_TYPE_INT32:
			{
				gint i;
				xmmsv_get_int (v, &i);
				print_info ("[%s] %s = %d", source, key, i);
				break;
			}
			default:
				print_info ("[%s] %s = (unknown data)", source, key);
				break;
			}
			xmmsv_dict_iter_next (it);
		}
	}
}

static void
print_padded_string (gint columns, gchar padchar, gboolean padright, const gchar *fmt, ...)
{
	gchar buf[1024];
	gchar *padstring;

	va_list ap;

	va_start (ap, fmt);
	g_vsnprintf (buf, 1024, fmt, ap);
	va_end (ap);

	padstring = g_strnfill (columns - g_utf8_strlen (buf, -1), padchar);

	if (padright) {
		print_info ("%s%s", buf, padstring);
	} else {
		print_info ("%s%s", padstring, buf);
	}

	g_free (padstring);
}

static gchar*
make_justified_columns_format (gint columns, const char type_first)
{
	int wd_id, wd_artist, wd_album, wd_title;
	gchar *buf = g_new (gchar, 128);

	/* count separators */
	columns -= 8;

	wd_id     = 5;
	wd_artist = (columns - wd_id) / 4;
	wd_album  = (columns - wd_id - wd_artist) / 2;
	wd_title  = (columns - wd_id - wd_artist - wd_album);

	g_snprintf (buf, 128, "%%-%d.%d%c| %%-%d.%ds | %%-%d.%ds | %%-%d.%ds",
	            wd_id, wd_id, type_first,
	            wd_artist, wd_artist, wd_album, wd_album, wd_title, wd_title);

	return buf;
}

void
format_pretty_list (xmmsc_connection_t *conn, GList *list)
{
	guint count = 0;
	GList *n;
	gint columns;
	gchar *format_header, *format_rows;

	columns = find_terminal_width ();
	format_header = make_justified_columns_format (columns, 's');
	format_rows   = make_justified_columns_format (columns, 'd');

	print_padded_string (columns, '-', TRUE, "-[Result]-");
	print_info (format_header, "Id", "Artist", "Album", "Title");

	for (n = list; n; n = g_list_next (n)) {
		const gchar *title;
		xmmsc_result_t *res;
		xmmsv_t *propdict, *val;
		gint mid = XPOINTER_TO_INT (n->data);

		if (!mid) {
			print_error ("Empty result!");
		}

		res = xmmsc_medialib_get_info (conn, mid);
		xmmsc_result_wait (res);
		propdict = xmmsc_result_get_value (res);
		val = xmmsv_propdict_to_dict (propdict, NULL);

		if (xmmsv_dict_entry_get_string (val, "title", &title)) {
			const gchar *artist, *album;
			if (!xmmsv_dict_entry_get_string (val, "artist", &artist)) {
				artist = "Unknown";
			}

			if (!xmmsv_dict_entry_get_string (val, "album", &album)) {
				album = "Unknown";
			}

			print_info (format_rows, mid, artist, album, title);
		} else {
			const gchar *url;
			xmmsv_dict_entry_get_string (val, "url", &url);
			if (url) {
				gchar *filename = g_path_get_basename (url);
				if (filename) {
					print_info ("%-5.5d| %s", mid, filename);
					g_free (filename);
				}
			}
		}

		count++;

		xmmsv_unref (val);
		xmmsc_result_unref (res);
	}

	print_padded_string (columns, '-', FALSE, "-[Count:%6.d]-----", count);

	g_free (format_header);
	g_free (format_rows);
}

/** Extracts collection name and namespace from a string.
 *
 * Note that name and namespace must be freed afterwards.
 */
gboolean
coll_read_collname (gchar *str, gchar **name, gchar **namespace)
{
	gchar **s;

	s = g_strsplit (str, "/", 0);
	g_assert (s);

	if (!s[0]) {
		g_strfreev (s);
		return FALSE;
	} else if (!s[1]) {
		/* No namespace, assume default */
		*name = g_strdup (s[0]);
		*namespace = g_strdup (CMD_COLL_DEFAULT_NAMESPACE);
	} else {
		*name = g_strdup (s[1]);
		*namespace = g_strdup (s[0]);
	}

	g_strfreev (s);
	return TRUE;
}

/** Return an escaped version of the string, with \-protected spaces
 *  outside of quotes.
 *
 *  The returned string must be freed manually afterwards.
 */
char *
string_escape (const char *s)
{
	gboolean escaped, quoted;
	gchar quote_char;
	gint i, w, len;
	char *res;

	/* Conservative length estimation */
	for (i = 0, len = 0; s[i] != '\0'; i++, len++) {
		if (s[i] == ' ' || s[i] == '\\') {
			len++;
		}
	}

	res = g_new0 (char, len + 1);

	/* Copy string, escape spaces outside of quotes */
	escaped = FALSE;
	quoted = FALSE;
	quote_char = 0;
	for (i = 0, w = 0; s[i] != '\0'; i++, w++) {
		if ((s[i] == ' ') && !quoted) {
			res[w] = '\\';
			w++;
		}

		if (!quoted && (s[i] == '"' || s[i] == '\'')) {
			quoted = TRUE;
			quote_char = s[i];
		} else if (quoted && s[i] == quote_char) {
			quoted = FALSE;
		}

		res[w] = s[i];
	}

	return res;
}
