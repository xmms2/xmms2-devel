/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#include <string.h>

#include <glib.h>

#include "matching_browse.h"

static void add_match (const gchar *path, gint isdir, GList **files);
static gchar *normalize_pattern (const gchar *pattern);
static gchar *find_next_directory (const gchar *path, const gchar *position);
static gboolean find_next_pattern (const gchar *haystack, const gchar **needle);
static GPatternSpec *build_match_pattern (const gchar *path, const gchar *ptr);
static gboolean resolve_pattern (xmmsc_connection_t *c, const gchar *pattern,
                                 GList **matches);

struct browse_entry_St {
	gchar *url;
	gint isdir;
};

void
browse_entry_get (browse_entry_t *entry, const gchar **url, gboolean *is_directory)
{
	*url = entry->url;
	*is_directory = entry->isdir;
}

void
browse_entry_free (browse_entry_t *entry)
{
	g_free (entry->url);
	g_free (entry);
}


/**
 * Resolve a pattern to a list of paths.
 *
 * First attempt to browse the path server side. If this does not
 * match any files, fall back to simply encoding the wild card characters
 * and return the pattern as a match.
 */
GList *
matching_browse (xmmsc_connection_t *conn, const gchar *pattern)
{
	GList *matches = NULL;
	const gchar *ptr;
	GString *str;

	/* Check if any of the patterns can be resolved via browsing */
	if (resolve_pattern (conn, pattern, &matches)) {
		return matches;
	}

	/* The pattern did not match any URLs when browsing so lets
	 * encode the wild card ('?', '*') characters as part of the URL.
	 */
	str = g_string_new (pattern);

	while (find_next_pattern (str->str, &ptr)) {
		gchar encoded[4];
		g_snprintf (encoded, sizeof (encoded), "%%%02x", *ptr);
		g_string_erase (str, ptr - str->str, 1);
		g_string_insert (str, ptr - str->str, encoded);
	}

	add_match (str->str, FALSE, &matches);

	g_string_free (str, FALSE);

	return matches;
}

/**
 * Resolve a URL pattern to a list of files.
 *
 * Scan pattern for occurrences of '?' and '*'. This pattern is then used to
 * construct match a specification that filters the browse result for each
 * directory depth. When a match is found and there are more wild cards left
 * in the pattern, it recursively calls itself until the whole pattern has
 * been resolved.
 */
static gboolean
resolve_pattern (xmmsc_connection_t *conn, const gchar *original_pattern,
                 GList **matches)
{
	gboolean match_found = FALSE;
	const gchar *ptr;
	gchar *directory, *pattern;
	GPatternSpec *spec;
	xmmsc_result_t *res;
	xmmsv_t *value;
	xmmsv_list_iter_t *it;

	pattern = normalize_pattern (original_pattern);

	/* Find the first pattern, or return end pointer */
	find_next_pattern (pattern, &ptr);

	/* Compile a match spec to filter the browse result. */
	spec = build_match_pattern (pattern, ptr);

	/* Find the parent directory closest to the pattern. */
	directory = find_next_directory (pattern, ptr);

	res = xmmsc_xform_media_browse_encoded (conn, directory);
	xmmsc_result_wait (res);

	g_free (directory);

	value = xmmsc_result_get_value (res);
	xmmsv_get_list_iter (value, &it);

	for (; xmmsv_list_iter_valid (it); xmmsv_list_iter_next (it)) {
		xmmsv_t *entry;
		const gchar *path;
		gint is_directory;

		xmmsv_list_iter_entry (it, &entry);

		xmmsv_dict_entry_get_int (entry, "isdir", &is_directory);
		xmmsv_dict_entry_get_string (entry, "path", &path);

		if (!g_pattern_match_string (spec, path)) {
			continue;
		}

		/* Check if path has been fully resolved, otherwise recurse. */
		if (strcmp (path, pattern) == 0) {
			add_match (path, is_directory, matches);
			match_found = TRUE;
		} else {
			gchar *npattern = g_strconcat (path, strchr (ptr, '/'), NULL);
			match_found |= resolve_pattern (conn, npattern, matches);
			g_free (npattern);
		}
	}

	xmmsc_result_unref (res);
	g_pattern_spec_free (spec);
	g_free (pattern);

	return match_found;
}

static void
add_match (const gchar *path, gint isdir, GList **files)
{
	browse_entry_t *entry;

	entry = g_new0 (browse_entry_t, 1);
	entry->url = g_strdup (path);
	entry->isdir = isdir;

	*files = g_list_prepend (*files, entry);
}

/**
 * Normalize pattern such that it has the same format as browse results.
 *
 * Strips trailing slashes as the browsing function indicates if an
 * entry is a directory via the 'isdir' attribute and will never contain
 * any trailing slash.
 */
static gchar *
normalize_pattern (const gchar *pattern)
{
	gchar *result;
	gint length;

	result = g_strdup (pattern);
	length = strlen (result);

	if (result[length - 1] == '/') {
		result[length - 1] = '\0';
	}

	return result;
}

static gboolean
find_next_pattern (const gchar *haystack, const gchar **needle)
{
	*needle = strpbrk (haystack, "*?");
	if (*needle == NULL) {
		*needle = haystack + strlen (haystack);
		return FALSE;
	}

	return TRUE;
}

static gchar *
find_next_directory (const gchar *path, const gchar *position)
{
	if (position == NULL) {
		return g_strdup (path);
	}

	while (position != path && *position != '/') {
		position--;
	}

	return g_strndup (path, position - path);
}

static GPatternSpec *
build_match_pattern (const gchar *path, const gchar *ptr)
{
	GPatternSpec *spec;
	const gchar *nslash;
	gchar *pattern;

	if (ptr == NULL) {
		return g_pattern_spec_new (path);
	}

	nslash = strchr (ptr, '/');
	if (nslash == NULL || nslash[1] == '\0') {
		nslash = ptr + strlen (ptr);
	} else {
		nslash = nslash - 1;
	}

	pattern = g_strndup (path, nslash - path + 1);
	spec = g_pattern_spec_new (pattern);
	g_free (pattern);

	return spec;
}
