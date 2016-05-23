/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "compat.h"
#include "utils.h"
#include "xmmscall.h"

gint
xmmsv_strcmp (xmmsv_t **a, xmmsv_t **b)
{
	const gchar *as, *bs;

	as = bs = NULL;

	xmmsv_get_string (*a, &as);
	xmmsv_get_string (*b, &bs);

	return g_strcmp0 (as, bs);
}

#define GOODCHAR(a) ((((a) >= 'a') && ((a) <= 'z')) ||	\
                     (((a) >= 'A') && ((a) <= 'Z')) ||	\
                     (((a) >= '0') && ((a) <= '9')) ||	\
                     ((a) == ':') || \
                     ((a) == '/') || \
                     ((a) == '-') || \
                     ((a) == '_') || \
                     ((a) == '.') || \
                     ((a) == '*') || \
                     ((a) == '?'))

void
xmmsv_print_value (const gchar *source, const gchar *key, xmmsv_t *val)
{
	if (source) {
		g_printf ("[%s] ", source);
	}
	if (key) {
		g_printf ("%s = ", key);
	}
	switch (xmmsv_get_type (val)) {
		case XMMSV_TYPE_INT32: {
			gint value;
			xmmsv_get_int (val, &value);
			g_printf ("%d\n", value);
			break;
		}
		case XMMSV_TYPE_FLOAT: {
			float value;
			xmmsv_get_float (val, &value);
			g_printf ("%f\n", value);
			break;
		}
		case XMMSV_TYPE_STRING: {
			const gchar *value;
			xmmsv_get_string (val, &value);
			g_printf ("%s\n", value);
			break;
		}
		case XMMSV_TYPE_LIST:
			g_printf (_("<list>\n"));
			break;
		case XMMSV_TYPE_DICT:
			g_printf (_("<dict>\n"));
			break;
		case XMMSV_TYPE_COLL:
			g_printf (_("<coll>\n"));
			break;
		case XMMSV_TYPE_BIN:
			g_printf (_("<bin>\n"));
			break;
		case XMMSV_TYPE_END:
			g_printf (_("<end>\n"));
			break;
		case XMMSV_TYPE_NONE:
			g_printf (_("<none>\n"));
			break;
		case XMMSV_TYPE_ERROR:
			g_printf (_("<error>\n"));
			break;
		default:
			g_printf (_("<unhandled type!>\n"));
			break;
	}
}

void
enrich_mediainfo (xmmsv_t *val)
{
	if (!xmmsv_dict_has_key (val, "title") && xmmsv_dict_has_key (val, "url")) {
		/* First decode the URL encoding */
		xmmsv_t *tmp, *v, *urlv;
		gchar *url = NULL;
		const gchar *filename = NULL;
		const unsigned char *burl;
		unsigned int blen;

		xmmsv_dict_get (val, "url", &v);

		tmp = xmmsv_decode_url (v);
		if (tmp && xmmsv_get_bin (tmp, &burl, &blen)) {
			url = g_malloc (blen + 1);
			memcpy (url, burl, blen);
			url[blen] = 0;
			xmmsv_unref (tmp);
			filename = strrchr (url, '/');
			if (!filename || !filename[1]) {
				filename = url;
			} else {
				filename = filename + 1;
			}
		}

		/* Let's see if the result is valid utf-8. This must be done
		 * since we don't know the charset of the binary string */
		if (filename && g_utf8_validate (filename, -1, NULL)) {
			/* If it's valid utf-8 we don't have any problem just
			 * printing it to the screen
			 */
			urlv = xmmsv_new_string (filename);
		} else if (filename) {
			/* Not valid utf-8 :-( We make a valid guess here that
			 * the string when it was encoded with URL it was in the
			 * same charset as we have on the terminal now.
			 *
			 * THIS MIGHT BE WRONG since different clients can have
			 * different charsets and DIFFERENT computers most likely
			 * have it.
			 */
			gchar *tmp2 = g_locale_to_utf8 (filename, -1, NULL, NULL, NULL);
			urlv = xmmsv_new_string (tmp2);
			g_free (tmp2);
		} else {
			/* Decoding the URL failed for some reason. That's not good. */
			urlv = xmmsv_new_string (_("?"));
		}

		xmmsv_dict_set (val, "title", urlv);
		xmmsv_unref (urlv);
		g_free (url);
	}
}

void
print_padding (gint length, const gchar padchar)
{
	while (length-- > 0) {
		g_printf ("%c", padchar);
	}
}

/* Returned string must be freed by the caller */
gchar *
format_time (guint64 duration, gboolean use_hours)
{
	guint64 hour, min, sec;
	gchar *time;

	/* +500 for rounding */
	sec = (duration+500) / 1000;
	min = sec / 60;
	sec = sec % 60;

	if (use_hours) {
		hour = min / 60;
		min = min % 60;
		time = g_strdup_printf ("%" G_GUINT64_FORMAT \
		                        ":%02" G_GUINT64_FORMAT \
		                        ":%02" G_GUINT64_FORMAT, hour, min, sec);
	} else {
		time = g_strdup_printf ("%02" G_GUINT64_FORMAT \
		                        ":%02" G_GUINT64_FORMAT, min, sec);
	}

	return time;
}

xmmsv_t *
xmmsv_coll_intersect_with_playlist (xmmsv_t *coll, const gchar *playlist)
{
	xmmsv_t *intersection, *reference;

	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_PLAYLISTS);
	xmmsv_coll_attribute_set_string (reference, "reference", playlist);

	intersection = xmmsv_new_coll (XMMS_COLLECTION_TYPE_INTERSECTION);
	xmmsv_coll_add_operand (intersection, coll);
	xmmsv_coll_add_operand (intersection, reference);

	xmmsv_unref (coll);
	xmmsv_unref (reference);

	return intersection;
}

/**
 * Apply the default ordering to a collection query, that is, its results will
 * be ordered by artist, and then album. To make this result more readable,
 * compilation albums are placed at the end to get proper album grouping.
 *
 * @param query A collection to be ordered.
 * @return The input collection with the default ordering.
 */
xmmsv_t *
xmmsv_coll_apply_default_order (xmmsv_t *query)
{
	xmmsv_t *compilation, *compilation_sorted;
	xmmsv_t *regular, *regular_sorted;
	xmmsv_t *complement, *concatenated;
	xmmsv_t *fields, *artist_order, *compilation_order, *regular_order;

	/* All various artists entries that match the user query. */
	compilation = xmmsv_new_coll (XMMS_COLLECTION_TYPE_MATCH);
	xmmsv_coll_add_operand (compilation, query);
	xmmsv_coll_attribute_set_string (compilation, "field", "compilation");
	xmmsv_coll_attribute_set_string (compilation, "value", "1");

	/* All entries that aren't various artists, or don't match the user query */
	complement = xmmsv_new_coll (XMMS_COLLECTION_TYPE_COMPLEMENT);
	xmmsv_coll_add_operand (complement, compilation);

	/* All entries that aren't various artists, and match the user query */
	regular = xmmsv_new_coll (XMMS_COLLECTION_TYPE_INTERSECTION);
	xmmsv_coll_add_operand (regular, query);
	xmmsv_coll_add_operand (regular, complement);
	xmmsv_unref (complement);

	compilation_order = xmmsv_build_list (
		XMMSV_LIST_ENTRY_STR ("album"),
		XMMSV_LIST_ENTRY_STR ("partofset"),
		XMMSV_LIST_ENTRY_STR ("tracknr"),
		XMMSV_LIST_END);

	compilation_sorted = xmmsv_coll_add_order_operators (compilation,
	                                                     compilation_order);
	xmmsv_unref (compilation);
	xmmsv_unref (compilation_order);

	fields = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("album_artist_sort"),
	                           XMMSV_LIST_ENTRY_STR ("album_artist"),
	                           XMMSV_LIST_ENTRY_STR ("artist_sort"),
	                           XMMSV_LIST_ENTRY_STR ("artist"),
	                           XMMSV_LIST_END);

	artist_order = xmmsv_build_dict (XMMSV_DICT_ENTRY_STR ("type", "value"),
	                                 XMMSV_DICT_ENTRY ("field", fields),
	                                 XMMSV_DICT_END);

	regular_order = xmmsv_build_list (
		XMMSV_LIST_ENTRY_STR ("album"),
		XMMSV_LIST_ENTRY_STR ("partofset"),
		XMMSV_LIST_ENTRY_STR ("tracknr"),
		XMMSV_LIST_END);

	xmmsv_list_insert (regular_order, 0, artist_order);
	xmmsv_unref (artist_order);

	regular_sorted = xmmsv_coll_add_order_operators (regular, regular_order);
	xmmsv_unref (regular);
	xmmsv_unref (regular_order);

	concatenated = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNION);
	xmmsv_coll_add_operand (concatenated, regular_sorted);
	xmmsv_unref (regular_sorted);
	xmmsv_coll_add_operand (concatenated, compilation_sorted);
	xmmsv_unref (compilation_sorted);

	return concatenated;
}

/** Try to retrieve and parse a collection pattern from stdin and return
 *  the collection after parsing.
 *
 *  @return The parsed collection from stdin or NULL if an error occured.
 */
xmmsv_t *
xmmsv_coll_from_stdin ()
{
	gchar *pattern = NULL;
	GError *error = NULL;
	xmmsv_t *ret = NULL;
	if (!g_file_get_contents("/dev/stdin", &pattern, NULL, &error)) {
		g_fprintf (stderr, "Error: Can't read pattern from stdin. ('%s')\n",
		           error->message);
		g_error_free (error);
	} else if (!xmmsv_coll_parse (pattern, &ret)) {
		g_printf (_("Error: failed to parse the pattern!\n"));
	}

	g_free (pattern);

	return ret;
}

gchar *
decode_url (const gchar *string)
{
	gint i = 0, j = 0;
	gchar *url;

	url = g_strdup (string);
	if (!url)
		return NULL;

	while (url[i]) {
		guchar chr = url[i++];

		if (chr == '+') {
			chr = ' ';
		} else if (chr == '%') {
			gchar ts[3];
			gchar *t;

			ts[0] = url[i++];
			if (!ts[0])
				goto err;
			ts[1] = url[i++];
			if (!ts[1])
				goto err;
			ts[2] = '\0';

			chr = strtoul (ts, &t, 16);

			if (t != &ts[2])
				goto err;
		}

		url[j++] = chr;
	}

	url[j] = '\0';

	return url;

err:
	g_free (url);
	return NULL;
}

gchar *
encode_url (gchar *url)
{
	static const gchar hex[16] = "0123456789abcdef";
	gint i = 0, j = 0;
	gchar *res;

	res = g_new0 (gchar, strlen (url) * 3 + 1);
	if (!res)
		return NULL;

	for (i = 0; url[i]; i++) {
		guchar chr = url[i];
		if (GOODCHAR (chr)) {
			res[j++] = chr;
		} else if (chr == ' ') {
			res[j++] = '+';
		} else {
			res[j++] = '%';
			res[j++] = hex[((chr & 0xf0) >> 4)];
			res[j++] = hex[(chr & 0x0f)];
		}
	}

	return res;
}

/* Transform a path (possibly absolute or relative) into a valid XMMS2
 * path with protocol prefix, and applies a file test to it.
 * The resulting string must be freed manually.
 *
 * @return the path in URL format if the test passes, or NULL.
 */
gchar *
format_url (const gchar *path, GFileTest test)
{
	gchar rpath[XMMS_PATH_MAX];
	const gchar *p;
	gchar *url;

	/* Check if path matches "^[a-z]+://" */
	for (p = path; *p >= 'a' && *p <= 'z'; ++p);

	/* If this test passes, path is a valid url and
	 * p points past its "file://" prefix.
	 */
	if (!(*p == ':' && *(++p) == '/' && *(++p) == '/')) {

		/* This is not a valid URL, try to work with
		 * the absolute path.
		 */
		if (!x_realpath (path, rpath)) {
			return NULL;
		}

		if (!g_file_test (rpath, test)) {
			return NULL;
		}

		url = g_strconcat ("file://", rpath, NULL);
	} else {
		url = g_strdup (path);
	}

	return x_path2url (url);
}

#define MSEC_PER_SEC  (1000L)
#define MSEC_PER_MIN  (MSEC_PER_SEC * 60L)
#define MSEC_PER_HOUR (MSEC_PER_MIN * 60L)
#define MSEC_PER_DAY  (MSEC_PER_HOUR * 24L)

void
breakdown_timespan (int64_t span, gint *days, gint *hours,
                    gint *minutes, gint *seconds)
{
	*days = span / MSEC_PER_DAY;
	*hours = (span % MSEC_PER_DAY) / MSEC_PER_HOUR;
	*minutes = ((span % MSEC_PER_DAY) % MSEC_PER_HOUR) / MSEC_PER_MIN;
	*seconds = (((span % MSEC_PER_DAY) % MSEC_PER_HOUR) % MSEC_PER_MIN) / MSEC_PER_SEC;
}
