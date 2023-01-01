/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

/** @file
 * Miscellaneous internal utility functions specific to the daemon.
 */

#include <stdlib.h>
#include <glib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <xmmsc/xmmsc_util.h>
#include <xmms/xmms_util.h>
#include <xmmspriv/xmms_utils.h>
#include <xmmsc/xmmsc_strlist.h>

/**
 * Build path to file in xmms2 configuration directory.
 * @param first The first file or directory name in the path.
 * @param ... Additional file/directory names.
 * @return Absolute path to a file or directory.
 */
char *
xmms_build_path (const char *first, ...)
{
	va_list ap;
	gchar confdir[XMMS_PATH_MAX];
	gchar *ret, **vargv, **argv;

	g_return_val_if_fail (first, NULL);

	xmms_userconfdir_get (confdir, XMMS_PATH_MAX);

	va_start (ap, first);
	vargv = xmms_valist_to_strlist (first, ap);
	va_end (ap);

	argv = xmms_strlist_prepend_copy (vargv, confdir);

	ret = g_build_pathv (G_DIR_SEPARATOR_S, argv);
	xmms_strlist_destroy (vargv);
	xmms_strlist_destroy (argv);
	return ret;
}

static gchar *
path_get_body (const gchar *path)
{
	gchar *beg, *end;

	g_return_val_if_fail (path, NULL);

	beg = strstr (path, "://");

	if (!beg) {
		return g_strndup (path, strcspn (path, "/"));
	}

	beg += 3;
	end = strchr (beg, '/');

	if (!end) {
		return g_strdup (path);
	}

	return g_strndup (path, end - path);
}

/* g_path_get_dirname returns "file:" with "file:///foo.pls", while "file://"
   is wanted. */
static gchar *
path_get_dirname (const gchar *path)
{
	guint i, n = 0;

	g_return_val_if_fail (path, NULL);

	for (i = 0; path[i] ; i++) {
		if (path[i] == '/') {
			n = i;
		}
	}

	return g_strndup (path, n);
}

gchar *
xmms_build_playlist_url (const gchar *plspath, const gchar *file)
{
	gchar *url;
	gchar *path;

	g_return_val_if_fail (plspath, NULL);
	g_return_val_if_fail (file, NULL);

	if (strstr (file, "://") != NULL) {
		return g_strdup (file);
	}

	if (file[0] == '/') {
		path = path_get_body (plspath);
		url = g_strconcat (path, file, NULL);
	} else {
		path = path_get_dirname (plspath);
		url = g_strconcat (path, "/", file, NULL);
	}

	g_free (path);
	return url;
}

gint
xmms_natcmp_len (const gchar *str1, gint len1, const gchar *str2, gint len2)
{
	gchar *tmp1, *tmp2, *tmp3, *tmp4;
	gint res;

	/* FIXME: Implement a less allocation-happy variant */
	tmp1 = g_utf8_casefold (str1, len1);
	tmp2 = g_utf8_casefold (str2, len2);

	tmp3 = g_utf8_collate_key_for_filename (tmp1, -1);
	tmp4 = g_utf8_collate_key_for_filename (tmp2, -1);

	res = strcmp (tmp3, tmp4);

	g_free (tmp1);
	g_free (tmp2);
	g_free (tmp3);
	g_free (tmp4);

	return res;
}

gint
xmms_natcmp (const gchar *str1, const gchar *str2)
{
	return xmms_natcmp_len (str1, -1, str2, -1);
}

/**
 * Case insensitive version of g_str_equal.
 * @param v1 first string
 * @param v2 second string
 * @return TRUE if first string equals the seconds tring, otherwise FALSE
 */
gboolean
xmms_strcase_equal (gconstpointer v1, gconstpointer v2)
{
	const gchar *string1 = v1;
	const gchar *string2 = v2;

	return strcasecmp (string1, string2) == 0;
}

/**
 * Case insensitive version of g_str_hash.
 * @param v a string
 * @return the case insensitive hash code of the string
 */
guint
xmms_strcase_hash (gconstpointer v)
{
	const signed char *p;
	guint32 h = 5381;

	for (p = v; *p != '\0'; p++)
		h = (h << 5) + h + (((*p <= 'Z') && (*p >= 'A')) ? *p + 32 : *p );

	return h;
}

