/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

#include "xmmsc/xmmsv.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_errorcodes.h"
#include "xmmsc/xmmsc_stdbool.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmspriv/xmmsv.h"
#include "xmmspriv/xmms_list.h"

/** @file */

/* Default source preferences for accessing "propdicts" */
const char *xmmsv_default_source_pref[] = {
	"server",
	"client/*",
	"plugin/playlist",
	"plugin/id3v2",
	"plugin/segment",
	"plugin/*",
	"*",
	NULL
};

static void xmmsv_free (xmmsv_t *val);

/**
 * Allocates a new empty #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_none (void)
{
	xmmsv_t *val = xmmsv_new (XMMSV_TYPE_NONE);
	return val;
}

/**
 * Allocates a new error #xmmsv_t.
 * @param s The error message to store in the #xmmsv_t. The
 * string is copied in the value.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_error (const char *errstr)
{
	xmmsv_t *val = xmmsv_new (XMMSV_TYPE_ERROR);

	if (val) {
		val->value.error = strdup (errstr);
	}

	return val;
}

/**
 * Allocates a new integer #xmmsv_t.
 * @param i The value to store in the #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_int (int32_t i)
{
	xmmsv_t *val = xmmsv_new (XMMSV_TYPE_INT32);

	if (val) {
		val->value.int32 = i;
	}

	return val;
}

/**
 * Allocates a new string #xmmsv_t.
 * @param s The value to store in the #xmmsv_t. The string is
 * copied in the value.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_string (const char *s)
{
	xmmsv_t *val;

	x_return_val_if_fail (s, NULL);
	x_return_val_if_fail (xmmsv_utf8_validate (s), NULL);

	val = xmmsv_new (XMMSV_TYPE_STRING);
	if (val) {
		val->value.string = strdup (s);
	}

	return val;
}

/**
 * Allocates a new collection #xmmsv_t.
 * @param s The value to store in the #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_coll (xmmsv_coll_t *c)
{
	xmmsv_t *val;

	x_return_val_if_fail (c, NULL);

	val = xmmsv_new (XMMSV_TYPE_COLL);
	if (val) {
		val->value.coll = c;
		xmmsv_coll_ref (c);
	}

	return val;
}

/**
 * Allocates a new binary data #xmmsv_t.
 * @param data The data to store in the #xmmsv_t.
 * @param len The size of the data.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_bin (const unsigned char *data, unsigned int len)
{
	xmmsv_t *val = xmmsv_new (XMMSV_TYPE_BIN);

	if (val) {
		/* copy the data! */
		val->value.bin.data = x_malloc (len);
		if (!val->value.bin.data) {
			free (val);
			x_oom ();
			return NULL;
		}
		memcpy (val->value.bin.data, data, len);
		val->value.bin.len = len;
	}

	return val;
}

/**
 * References the #xmmsv_t
 *
 * @param val the value to reference.
 * @return val
 */
xmmsv_t *
xmmsv_ref (xmmsv_t *val)
{
	x_return_val_if_fail (val, NULL);
	val->ref++;

	return val;
}

/**
 * Decreases the references for the #xmmsv_t
 * When the number of references reaches 0 it will
 * be freed. And thus all data you extracted from it
 * will be deallocated.
 */
void
xmmsv_unref (xmmsv_t *val)
{
	x_return_if_fail (val);
	x_api_error_if (val->ref < 1, "with a freed value",);

	val->ref--;
	if (val->ref == 0) {
		xmmsv_free (val);
	}
}


/**
 * Allocates new #xmmsv_t and references it.
 * @internal
 */
xmmsv_t *
xmmsv_new (xmmsv_type_t type)
{
	xmmsv_t *val;

	val = x_new0 (xmmsv_t, 1);
	if (!val) {
		x_oom ();
		return NULL;
	}

	val->type = type;

	return xmmsv_ref (val);
}

/**
 * Free a #xmmsv_t along with its internal data.
 * @internal
 */
static void
xmmsv_free (xmmsv_t *val)
{
	x_return_if_fail (val);

	switch (val->type) {
		case XMMSV_TYPE_NONE :
		case XMMSV_TYPE_END :
		case XMMSV_TYPE_INT32 :
			break;
		case XMMSV_TYPE_ERROR :
			free (val->value.error);
			val->value.error = NULL;
			break;
		case XMMSV_TYPE_STRING :
			free (val->value.string);
			val->value.string = NULL;
			break;
		case XMMSV_TYPE_COLL:
			xmmsv_coll_unref (val->value.coll);
			val->value.coll = NULL;
			break;
		case XMMSV_TYPE_BIN :
			free (val->value.bin.data);
			val->value.bin.len = 0;
			break;
		case XMMSV_TYPE_LIST:
			_xmmsv_list_free (val->value.list);
			val->value.list = NULL;
			break;
		case XMMSV_TYPE_DICT:
			_xmmsv_dict_free (val->value.dict);
			val->value.dict = NULL;
			break;
		case XMMSV_TYPE_BITBUFFER:
			if (!val->value.bit.ro && val->value.bit.buf) {
				free (val->value.bit.buf);
			}
			val->value.bit.buf = NULL;
			break;
	}

	free (val);
}


/**
 * Get the type of the value.
 *
 * @param val a #xmmsv_t to get the type from.
 * @returns The data type in the value.
 */
xmmsv_type_t
xmmsv_get_type (const xmmsv_t *val)
{
	x_api_error_if (!val, "NULL value",
	                XMMSV_TYPE_NONE);

	return val->type;
}

/**
 * Check if value is of specified type.
 *
 * @param val #xmmsv_t to check.
 * @param t #xmmsv_type_t to check for.
 * @return 1 if value is of specified type, 0 otherwise.
 */
int
xmmsv_is_type (const xmmsv_t *val, xmmsv_type_t t)
{
	x_api_error_if (!val, "NULL value", 0);

	return (xmmsv_get_type (val) == t);
}

/* Merely legacy aliases */

/**
 * Check if the value stores an error.
 *
 * @param val a #xmmsv_t
 * @return 1 if error was encountered, 0 otherwise.
 */
int
xmmsv_is_error (const xmmsv_t *val)
{
	return xmmsv_is_type (val, XMMSV_TYPE_ERROR);
}

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
 * Retrieves an error string describing the server error from the
 * value.
 *
 * @param val a #xmmsv_t containing a integer.
 * @param r the return error.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_error (const xmmsv_t *val, const char **r)
{
	if (!val || val->type != XMMSV_TYPE_ERROR) {
		return 0;
	}

	*r = val->value.error;

	return 1;
}

/**
 * Retrieves a signed integer from the value.
 *
 * @param val a #xmmsv_t containing an integer.
 * @param r the return integer.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_int (const xmmsv_t *val, int32_t *r)
{
	if (!val || val->type != XMMSV_TYPE_INT32) {
		return 0;
	}

	*r = val->value.int32;

	return 1;
}

/**
 * Retrieves a string from the value.
 *
 * @param val a #xmmsv_t containing a string.
 * @param r the return string. This string is owned by the value and
 * will be freed when the value is freed.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_string (const xmmsv_t *val, const char **r)
{
	if (!val || val->type != XMMSV_TYPE_STRING) {
		return 0;
	}

	*r = val->value.string;

	return 1;
}

/**
 * Retrieves a collection from the value.
 *
 * @param val a #xmmsv_t containing a collection.
 * @param c the return collection. This collection is owned by the
 * value and will be unref'd when the value is freed.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_coll (const xmmsv_t *val, xmmsv_coll_t **c)
{
	if (!val || val->type != XMMSV_TYPE_COLL) {
		return 0;
	}

	*c = val->value.coll;

	return 1;
}

/**
 * Retrieves binary data from the value.
 *
 * @param val a #xmmsv_t containing a string.
 * @param r the return data. This data is owned by the value and will
 * be freed when the value is freed.
 * @param rlen the return length of data.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_bin (const xmmsv_t *val, const unsigned char **r, unsigned int *rlen)
{
	if (!val || val->type != XMMSV_TYPE_BIN) {
		return 0;
	}

	*r = val->value.bin.data;
	*rlen = val->value.bin.len;

	return 1;
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
			int duration;

			if (xmmsv_dict_iter_find (it, "duration")) {
				xmmsv_dict_iter_pair (it, NULL, &v);
				xmmsv_get_int (v, &duration);
			} else {
				duration = 0;
			}

			if (!duration) {
				strncat (target, "00", len - strlen (target) - 1);
			} else {
				char seconds[10];
				/* rounding */
				duration += 500;
				snprintf (seconds, sizeof (seconds), "%02d", (duration/1000)%60);
				strncat (target, seconds, len - strlen (target) - 1);
			}
		} else if (strcmp (key, "minutes") == 0) {
			int duration;

			if (xmmsv_dict_iter_find (it, "duration")) {
				xmmsv_dict_iter_pair (it, NULL, &v);
				xmmsv_get_int (v, &duration);
			} else {
				duration = 0;
			}

			if (!duration) {
				strncat (target, "00", len - strlen (target) - 1);
			} else {
				char minutes[10];
				/* rounding */
				duration += 500;
				snprintf (minutes, sizeof (minutes), "%02d", duration/60000);
				strncat (target, minutes, len - strlen (target) - 1);
			}
		} else {
			const char *result = NULL;
			char tmp[17];

			if (xmmsv_dict_iter_find (it, key)) {
				xmmsv_dict_iter_pair (it, NULL, &v);

				xmmsv_type_t type = xmmsv_get_type (v);
				if (type == XMMSV_TYPE_STRING) {
					xmmsv_get_string (v, &result);
				} else if (type == XMMSV_TYPE_INT32) {
					int32_t i;
					xmmsv_get_int (v, &i);
					snprintf (tmp, 12, "%d", i);
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
