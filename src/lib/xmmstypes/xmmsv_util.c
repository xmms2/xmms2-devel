/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <xmmscpriv/xmmsv.h>
#include <xmmscpriv/xmmsc_util.h>

static int
source_match_pattern (const char *source, const char *pattern)
{
	int match = 0;
	int lpos = strlen (pattern) - 1;

	if (strcasecmp (pattern, source) == 0) {
		match = 1;
	} else if (lpos >= 0 && pattern[lpos] == '*' &&
	           (lpos == 0 || strncasecmp (source, pattern, lpos) == 0)) {
		match = 1;
	}

	return match;
}

/* Return the index of the source in the source prefs list, or -1 if
 * no match.
 */
static int
find_match_index (const char *source, const char **src_prefs)
{
	int i, match = -1;

	for (i = 0; src_prefs[i]; i++) {
		if (source_match_pattern (source, src_prefs[i])) {
			match = i;
			break;
		}
	}

	return match;
}

/**
 * Helper function to transform a key-source-value dict-of-dict
 * #xmmsv_t (formerly a propdict) to a regular key-value dict, given a
 * list of source preference.
 *
 * @param propdict A key-source-value dict-of-dict #xmmsv_t.
 * @param src_prefs A list of source names or patterns. Must be
 *                  NULL-terminated. If this argument is NULL, the
 *                  default source preferences is used.
 * @return An #xmmsv_t containing a simple key-value dict. Must be
 *         unreffed manually when done.
 */
xmmsv_t *
xmmsv_propdict_to_dict (xmmsv_t *propdict, const char **src_prefs)
{
	xmmsv_t *dict, *source_dict, *value, *best_value;
	xmmsv_dict_iter_t *key_it, *source_it;
	const char *key, *source;
	const char **local_prefs;
	int match_index, best_index;

	dict = xmmsv_new_dict ();

	local_prefs = src_prefs ? src_prefs : xmmsv_default_source_pref;

	xmmsv_get_dict_iter (propdict, &key_it);
	while (xmmsv_dict_iter_valid (key_it)) {
		xmmsv_dict_iter_pair (key_it, &key, &source_dict);

		best_value = NULL;
		best_index = -1;
		xmmsv_get_dict_iter (source_dict, &source_it);
		while (xmmsv_dict_iter_valid (source_it)) {
			xmmsv_dict_iter_pair (source_it, &source, &value);
			match_index = find_match_index (source, local_prefs);
			/* keep first match or better match */
			if (match_index >= 0 && (best_index < 0 ||
			                         match_index < best_index)) {
				best_value = value;
				best_index = match_index;
			}
			xmmsv_dict_iter_next (source_it);
		}

		/* Note: we do not insert a key-value pair if no source matches */
		if (best_value) {
			xmmsv_dict_set (dict, key, best_value);
		}

		xmmsv_dict_iter_next (key_it);
	}

	return dict;
}

/**
 * Decode an URL-encoded string.
 *
 * Some strings (currently only the url of media) has no known
 * encoding, and must be encoded in an UTF-8 clean way. This is done
 * similar to the url encoding web browsers do. This functions decodes
 * a string encoded in that way. OBSERVE that the decoded string HAS
 * NO KNOWN ENCODING and you cannot display it on screen in a 100%
 * guaranteed correct way (a good heuristic is to try to validate the
 * decoded string as UTF-8, and if it validates assume that it is an
 * UTF-8 encoded string, and otherwise fall back to some other
 * encoding).
 *
 * Do not use this function if you don't understand the
 * implications. The best thing is not to try to display the url at
 * all.
 *
 * Note that the fact that the string has NO KNOWN ENCODING and CAN
 * NOT BE DISPLAYED does not stop you from open the file if it is a
 * local file (if it starts with "file://").
 *
 * @param url the #xmmsv_t containing a url-encoded string
 * @return a new #xmmsv_t containing the decoded string as a XMMSV_BIN or NULL on failure
 *
 */
xmmsv_t *
xmmsv_decode_url (const xmmsv_t *inv)
{
	int i = 0, j = 0;
	const char *ins;
	unsigned char *url;
	xmmsv_t *ret;

	if (!xmmsv_get_string (inv, &ins)) {
		return NULL;
	}

	url = x_malloc (strlen (ins));
	if (!url) {
		x_oom ();
		return NULL;
	}

	while (ins[i]) {
		unsigned char chr = ins[i++];

		if (chr == '+') {
			chr = ' ';
		} else if (chr == '%') {
			char ts[3];
			char *t;

			ts[0] = ins[i++];
			if (!ts[0])
				goto err;
			ts[1] = ins[i++];
			if (!ts[1])
				goto err;
			ts[2] = '\0';

			chr = strtoul (ts, &t, 16);

			if (t != &ts[2])
				goto err;
		}

		url[j++] = chr;
	}

	ret = xmmsv_new_bin (url, j);
	free (url);

	return ret;

err:
	free (url);
	return NULL;
}

/**
 * This function will make a pretty string about the information in
 * xmmsv dict.
 *
 * @param target A allocated char *
 * @param len Length of target
 * @param fmt A format string to use. You can insert items from the dict by
 * using specialformat "${field}".
 * @param val The #xmmsv_t that contains the dict.
 *
 * @returns The number of chars written to target
 */
int
xmmsv_dict_format (char *target, int len, const char *fmt, xmmsv_t *val)
{
	const char *pos;

	if (!target) {
		return 0;
	}

	if (!fmt) {
		return 0;
	}

	memset (target, 0, len);

	pos = fmt;
	while (strlen (target) + 1 < len) {
		char *next_key, *key, *end;
		int keylen;
		xmmsv_dict_iter_t *it;
		xmmsv_t *v;

		next_key = strstr (pos, "${");
		if (!next_key) {
			strncat (target, pos, len - strlen (target) - 1);
			break;
		}

		strncat (target, pos, MIN (next_key - pos, len - strlen (target) - 1));
		keylen = strcspn (next_key + 2, "}");
		key = malloc (keylen + 1);

		if (!key) {
			fprintf (stderr, "Unable to allocate %u bytes of memory, OOM?", keylen);
			break;
		}

		memset (key, 0, keylen + 1);
		strncpy (key, next_key + 2, keylen);

		xmmsv_get_dict_iter (val, &it);

		if (strcmp (key, "seconds") == 0) {
			int64_t duration;

			if (xmmsv_dict_iter_find (it, "duration")) {
				xmmsv_dict_iter_pair (it, NULL, &v);
				xmmsv_get_int (v, &duration);
			} else {
				duration = 0;
			}

			if (!duration) {
				strncat (target, "00", len - strlen (target) - 1);
			} else {
				char seconds[21];
				/* rounding */
				duration += 500;
				snprintf (seconds, sizeof (seconds), "%02" PRId64, (duration/1000)%60);
				strncat (target, seconds, len - strlen (target) - 1);
			}
		} else if (strcmp (key, "minutes") == 0) {
			int64_t duration;

			if (xmmsv_dict_iter_find (it, "duration")) {
				xmmsv_dict_iter_pair (it, NULL, &v);
				xmmsv_get_int (v, &duration);
			} else {
				duration = 0;
			}

			if (!duration) {
				strncat (target, "00", len - strlen (target) - 1);
			} else {
				char minutes[21];
				/* rounding */
				duration += 500;
				snprintf (minutes, sizeof (minutes), "%02" PRId64, duration/60000);
				strncat (target, minutes, len - strlen (target) - 1);
			}
		} else {
			const char *result = NULL;
			char tmp[21];

			if (xmmsv_dict_iter_find (it, key)) {
				xmmsv_dict_iter_pair (it, NULL, &v);

				xmmsv_type_t type = xmmsv_get_type (v);
				if (type == XMMSV_TYPE_STRING) {
					xmmsv_get_string (v, &result);
				} else if (type == XMMSV_TYPE_INT64) {
					int64_t i;
					xmmsv_get_int (v, &i);
					snprintf (tmp, 21, "%" PRId64, i);
					result = tmp;
				} else if (type == XMMSV_TYPE_FLOAT) {
					float f;
					xmmsv_get_float (v, &f);
					snprintf (tmp, 12, "%.6f", f);
					result = tmp;
				}
			}

			if (result)
				strncat (target, result, len - strlen (target) - 1);
		}

		free (key);
		end = strchr (next_key, '}');

		if (!end) {
			break;
		}

		pos = end + 1;
	}

	return strlen (target);
}

static int
_xmmsv_utf8_charlen (unsigned char c)
{
	if ((c & 0x80) == 0) {
		return 1;
	} else if ((c & 0x60) == 0x40) {
		return 2;
	} else if ((c & 0x70) == 0x60) {
		return 3;
	} else if ((c & 0x78) == 0x70) {
		return 4;
	}
	return 0;
}

/**
 * Check if a string is valid UTF-8.
 *
 */
int
xmmsv_utf8_validate (const char *str)
{
	int i = 0;

	for (;;) {
		unsigned char c = str[i++];
		int l;
		if (!c) {
			/* NUL - end of string */
			return 1;
		}

		l = _xmmsv_utf8_charlen (c);
		if (l == 0)
			return 0;
		while (l-- > 1) {
			if ((str[i++] & 0xC0) != 0x80)
				return 0;
		}
	}
}
