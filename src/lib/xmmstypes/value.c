/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "xmmsc/xmmsc_value.h"
#include "xmmsclient/xmmsclient.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_errorcodes.h"
#include "xmmspriv/xmms_list.h"


/* Default source preferences for accessing "propdicts" */
const char *default_source_pref[] = {
	"server",
	"client/*",
	"plugin/id3v2",
	"plugin/segment",
	"plugin/*",
	"*",
	NULL
};


typedef struct xmmsv_list_St xmmsv_list_t;
typedef struct xmmsv_dict_St xmmsv_dict_t;


typedef struct xmmsv_bin_St {
	unsigned char *data;
	uint32_t len;
} xmmsv_bin_t;

struct xmmsv_list_St {
	xmmsv_t **list;
	size_t size;
	size_t allocated;
	x_list_t *iterators;
};

static xmmsv_list_t *xmmsv_list_new (void);
static void xmmsv_list_free (xmmsv_list_t *l);
static int xmmsv_list_resize (xmmsv_list_t *l, size_t newsize);
static int _xmmsv_list_insert (xmmsv_list_t *l, int pos, xmmsv_t *val);
static int _xmmsv_list_append (xmmsv_list_t *l, xmmsv_t *val);
static int _xmmsv_list_remove (xmmsv_list_t *l, int pos);
static void _xmmsv_list_clear (xmmsv_list_t *l);

static xmmsv_dict_t *xmmsv_dict_new (void);
static void xmmsv_dict_free (xmmsv_dict_t *dict);


struct xmmsv_list_iter_St {
	xmmsv_list_t *parent;
	unsigned int position;
};

static xmmsv_list_iter_t *xmmsv_list_iter_new (xmmsv_list_t *l);
static void xmmsv_list_iter_free (xmmsv_list_iter_t *it);


static xmmsv_dict_iter_t *xmmsv_dict_iter_new (xmmsv_dict_t *d);
static void xmmsv_dict_iter_free (xmmsv_dict_iter_t *it);



struct xmmsv_St {
	union {
		void *generic;
		char *error;
		uint32_t uint32;
		int32_t int32;
		char *string;
		xmmsv_coll_t *coll;
		xmmsv_bin_t bin;
		xmmsv_list_t *list;
		xmmsv_dict_t *dict;
	} value;
	xmmsv_type_t type;

	int ref;  /* refcounting */
};


static xmmsv_t *xmmsv_new (xmmsv_type_t type);
static void xmmsv_free (xmmsv_t *val);
static void value_data_free (xmmsv_t *val);
static int get_absolute_position (int pos, unsigned int size, unsigned int *abspos);



/**
 * @defgroup ValueType ValueType
 * @ingroup Values
 * @brief The API to be used to work with value objects.
 *
 * @{
 */

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
 * Allocates a new unsigned integer #xmmsv_t.
 * @param u The value to store in the #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_uint (uint32_t u)
{
	xmmsv_t *val = xmmsv_new (XMMSV_TYPE_UINT32);

	if (val) {
		val->value.uint32 = u;
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
xmmsv_new_bin (unsigned char *data, unsigned int len)
{
	xmmsv_t *val = xmmsv_new (XMMSV_TYPE_BIN);

	if (val) {
		/* copy the data! */
		val->value.bin.data = x_malloc (len);
		if (!val->value.bin.data) {
			x_oom ();
			return NULL;
		}
		memcpy (val->value.bin.data, data, len);
		val->value.bin.len = len;
	}

	return val;
}

/**
 * Allocates a new list #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_list (void)
{
	xmmsv_t *val = xmmsv_new (XMMSV_TYPE_LIST);

	if (val) {
		val->value.list = xmmsv_list_new ();
	}

	return val;
}

/**
 * Allocates a new dict #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_dict (void)
{
	xmmsv_t *val = xmmsv_new (XMMSV_TYPE_DICT);

	if (val) {
		val->value.dict = xmmsv_dict_new ();
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
static xmmsv_t *
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

	value_data_free (val);

	free (val);
}


static void
value_data_free (xmmsv_t *val)
{
	x_return_if_fail (val);

	switch (val->type) {
		case XMMSV_TYPE_NONE :
		case XMMSV_TYPE_END :
		case XMMSV_TYPE_UINT32 :
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
			xmmsv_list_free (val->value.list);
			val->value.list = NULL;
			break;
		case XMMSV_TYPE_DICT:
			xmmsv_dict_free (val->value.dict);
			val->value.dict = NULL;
			break;
	}

	val->type = XMMSV_TYPE_NONE;
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
	return !val || xmmsv_get_type (val) == XMMSV_TYPE_ERROR;
}

/**
 * Check if the value stores a list.
 *
 * @param val a #xmmsv_t
 * @return 1 if value stores a list, 0 otherwise.
 */
int
xmmsv_is_list (const xmmsv_t *val)
{
	return xmmsv_get_type (val) == XMMSV_TYPE_LIST;
}

/**
 * Check if the value stores a dict.
 *
 * @param val a #xmmsv_t
 * @return 1 if value stores a dict, 0 otherwise.
 */
int
xmmsv_is_dict (const xmmsv_t *val)
{
	return xmmsv_get_type (val) == XMMSV_TYPE_DICT;
}

/**
 * Legacy alias to retrieve the error string from an
 * #xmmsv_t. Obsolete now, use #xmmsv_get_error instead!
 *
 * @param val an error #xmmsv_t
 * @return the error string if valid, NULL otherwise.
 */
const char *
xmmsv_get_error_old (const xmmsv_t *val)
{
	if (!val || val->type != XMMSV_TYPE_ERROR) {
		return NULL;
	}

	return val->value.error;
}

/**
 * Helper function to build a list #xmmsv_t containing the
 * strings from the input array.
 *
 * @param array An array of C strings. Must be NULL-terminated if num
 *              is -1.
 * @param num The optional number of elements to read from the array. Set to
 *            -1 if the array is NULL-terminated.
 * @return An #xmmsv_t containing the list of strings. Must be
 *         unreffed manually when done.
 */
xmmsv_t *
xmmsv_make_stringlist (char *array[], int num)
{
	xmmsv_t *list, *elem;
	int i;

	list = xmmsv_new_list ();
	if (array) {
		for (i = 0; (num >= 0 && i < num) || array[i]; i++) {
			elem = xmmsv_new_string (array[i]);
			xmmsv_list_append (list, elem);
			xmmsv_unref (elem);
		}
	}

	return list;
}

/**
 * Helper function to build a dict #xmmsv_t containing the
 * key-value string pairs from the input array.
 *
 * @param array A flat array of C string pairs (key1, val1, key2,
 *              val2, etc). Must be NULL-terminated.
 * @return An #xmmsv_t containing the list of strings. Must be
 *         unreffed manually when done.
 */
xmmsv_t *
xmmsv_make_dict (const char *array[])
{
	const char *key;
	xmmsv_t *dict, *value;
	int i;

	dict = xmmsv_new_dict ();
	if (array) {
		for (i = 0; array[i]; i += 2) {
			key = array[i];
			if (!array[i + 1]) {
				goto err;
			}
			value = xmmsv_new_string (array[i + 1]);
			xmmsv_dict_insert (dict, key, value);
			xmmsv_unref (value);
		}
	}

	return dict;

err:
	x_api_warning ("with invalid key/value pairs");
	xmmsv_unref (dict);
	return NULL;
}


xmmsv_type_t
xmmsv_get_dict_entry_type (xmmsv_t *val, const char *key)
{
	xmmsv_dict_iter_t *it;
	xmmsv_t *v;

	if (!val || !xmmsv_get_dict_iter (val, &it) ||
	    !xmmsv_dict_iter_seek (it, key)) {
		return 0;
	}

	xmmsv_dict_iter_pair (it, NULL, &v);

	xmmsv_dict_iter_free (it);

	return xmmsv_get_type (v);
}


/* macro-magically define legacy dict extractors */
#define GEN_COMPAT_DICT_EXTRACTOR_FUNC(typename, type) \
	int \
	xmmsv_get_dict_entry_##typename (xmmsv_t *val, const char *key, \
	                                      type *r) \
	{ \
		xmmsv_dict_iter_t *it; \
		xmmsv_t *v; \
		if (!val || !xmmsv_get_dict_iter (val, &it) || \
		    !xmmsv_dict_iter_seek (it, key)) { \
			return 0; \
		} \
		xmmsv_dict_iter_pair (it, NULL, &v); \
		xmmsv_dict_iter_free (it); \
		return xmmsv_get_##typename (v, r); \
	}

GEN_COMPAT_DICT_EXTRACTOR_FUNC (string, const char *)
GEN_COMPAT_DICT_EXTRACTOR_FUNC (int, int32_t)
GEN_COMPAT_DICT_EXTRACTOR_FUNC (uint, uint32_t)
GEN_COMPAT_DICT_EXTRACTOR_FUNC (collection, xmmsv_coll_t *)

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

	local_prefs = src_prefs ? src_prefs : default_source_pref;

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
			xmmsv_dict_insert (dict, key, best_value);
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
 * Retrieves a unsigned integer from the value.
 *
 * @param val a #xmmsv_t containing an unsigned integer.
 * @param r the return unsigned integer.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_uint (const xmmsv_t *val, uint32_t *r)
{
	if (!val || val->type != XMMSV_TYPE_UINT32)
		return 0;

	*r = val->value.uint32;

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
xmmsv_get_collection (const xmmsv_t *val, xmmsv_coll_t **c)
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
 * Retrieves a list iterator from a list #xmmsv_t.
 *
 * @param val a #xmmsv_t containing a list.
 * @param it An #xmmsv_list_iter_t that can be used to access the list
 *           data. The iterator will be freed when the value is freed.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_list_iter (const xmmsv_t *val, xmmsv_list_iter_t **it)
{
	xmmsv_list_iter_t *new_it;

	if (!val || val->type != XMMSV_TYPE_LIST) {
		*it = NULL;
		return 0;
	}

	new_it = xmmsv_list_iter_new (val->value.list);
	if (!new_it) {
		*it = NULL;
		return 0;
	}

	*it = new_it;

	return 1;
}

/**
 * Retrieves a dict iterator from a dict #xmmsv_t.
 *
 * @param val a #xmmsv_t containing a dict.
 * @param it An #xmmsv_dict_iter_t that can be used to access the dict
 *           data. The iterator will be freed when the value is freed.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_dict_iter (const xmmsv_t *val, xmmsv_dict_iter_t **it)
{
	xmmsv_dict_iter_t *new_it;

	if (!val || val->type != XMMSV_TYPE_DICT) {
		*it = NULL;
		return 0;
	}

	new_it = xmmsv_dict_iter_new (val->value.dict);
	if (!new_it) {
		*it = NULL;
		return 0;
	}

	*it = new_it;

	return 1;
}


/* List stuff */

static xmmsv_list_t *
xmmsv_list_new (void)
{
	xmmsv_list_t *list;

	list = x_new0 (xmmsv_list_t, 1);
	if (!list) {
		x_oom ();
		return NULL;
	}

	/* list is all empty for now! */

	return list;
}

static void
xmmsv_list_free (xmmsv_list_t *l)
{
	xmmsv_list_iter_t *it;
	size_t i;

	/* free iterators */
	while (l->iterators) {
		it = (xmmsv_list_iter_t *) l->iterators->data;
		xmmsv_list_iter_free (it);
	}

	/* unref contents */
	for (i = 0; i < l->size; i++) {
		xmmsv_unref (l->list[i]);
	}

	free (l->list);
	free (l);
}

static int
xmmsv_list_resize (xmmsv_list_t *l, size_t newsize)
{
	xmmsv_t **newmem;

	newmem = realloc (l->list, newsize * sizeof (xmmsv_t *));

	if (newsize != 0 && newmem == NULL) {
		x_oom ();
		return 0;
	}

	l->list = newmem;
	l->allocated = newsize;

	return 1;
}

static int
_xmmsv_list_insert (xmmsv_list_t *l, int pos, xmmsv_t *val)
{
	unsigned int abspos;
	xmmsv_list_iter_t *it;
	x_list_t *n;

	if (!get_absolute_position (pos, l->size, &abspos)) {
		return 0;
	}

	/* We need more memory, reallocate */
	if (l->size == l->allocated) {
		int success;
		size_t double_size;
		if (l->allocated > 0) {
			double_size = l->allocated << 1;
		} else {
			double_size = 1;
		}
		success = xmmsv_list_resize (l, double_size);
		x_return_val_if_fail (success, 0);
	}

	/* move existing items out of the way */
	if (l->size > abspos) {
		memmove (l->list + abspos + 1, l->list + abspos,
		         (l->size - abspos) * sizeof (xmmsv_t *));
	}

	l->list[abspos] = xmmsv_ref (val);
	l->size++;

	/* update iterators pos */
	for (n = l->iterators; n; n = n->next) {
		it = (xmmsv_list_iter_t *) n->data;
		if (it->position > abspos) {
			it->position++;
		}
	}

	return 1;
}

static int
_xmmsv_list_append (xmmsv_list_t *l, xmmsv_t *val)
{
	return _xmmsv_list_insert (l, l->size, val);
}

static int
_xmmsv_list_remove (xmmsv_list_t *l, int pos)
{
	unsigned int abspos;
	xmmsv_list_iter_t *it;
	size_t half_size;
	x_list_t *n;

	/* prevent removing after the last element */
	x_return_val_if_fail (pos < l->size, 0);
	if (!get_absolute_position (pos, l->size, &abspos)) {
		return 0;
	}

	xmmsv_unref (l->list[abspos]);

	l->size--;

	/* fill the gap */
	if (abspos < l->size) {
		memmove (l->list + abspos, l->list + abspos + 1,
		         (l->size - abspos) * sizeof (xmmsv_t *));
	}

	/* Reduce memory usage by two if possible */
	half_size = l->allocated >> 1;
	if (l->size <= half_size) {
		int success;
		success = xmmsv_list_resize (l, half_size);
		x_return_val_if_fail (success, 0);
	}

	/* update iterator pos */
	for (n = l->iterators; n; n = n->next) {
		it = (xmmsv_list_iter_t *) n->data;
		if (it->position > abspos) {
			it->position--;
		}
	}

	return 1;
}

static void
_xmmsv_list_clear (xmmsv_list_t *l)
{
	xmmsv_list_iter_t *it;
	x_list_t *n;
	int i;

	/* unref all stored values */
	for (i = 0; i < l->size; i++) {
		xmmsv_unref (l->list[i]);
	}

	/* free list, declare empty */
	free (l->list);
	l->list = NULL;

	l->size = 0;
	l->allocated = 0;

	/* reset iterator pos */
	for (n = l->iterators; n; n = n->next) {
		it = (xmmsv_list_iter_t *) n->data;
		it->position = 0;
	}
}

/**
 * Get the element at the given position in the list #xmmsv_t. This
 * function does not increase the refcount of the element, the
 * reference is still owned by the list.
 *
 * @param listv A #xmmsv_t containing a list.
 * @param pos The position in the list. If negative, start counting
 *            from the end (-1 is the last element, etc).
 * @param val Pointer set to a borrowed reference to the element at
 *            the given position in the list.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_get (xmmsv_t *listv, int pos, xmmsv_t **val)
{
	unsigned int abspos;
	xmmsv_list_t *l;

	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_list (listv), 0);

	l = listv->value.list;

	/* prevent accessing after the last element */
	if (!get_absolute_position (pos, l->size, &abspos)) {
		return 0;
	}

	if (val) {
		*val = l->list[abspos];
	}

	return 1;
}

/**
 * Set the element at the given position in the list #xmmsv_t.
 *
 * @param listv A #xmmsv_t containing a list.
 * @param pos The position in the list. If negative, start counting
 *            from the end (-1 is the last element, etc).
 * @param val The element to put at the given position in the list.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_set (xmmsv_t *listv, int pos, xmmsv_t *val)
{
	unsigned int abspos;
	xmmsv_t *old_val;
	xmmsv_list_t *l;

	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (val, 0);
	x_return_val_if_fail (xmmsv_is_list (listv), 0);

	l = listv->value.list;

	/* prevent accessing after the last element */
	if (!get_absolute_position (pos, l->size, &abspos)) {
		return 0;
	}

	old_val = l->list[abspos];
	l->list[abspos] = xmmsv_ref (val);
	xmmsv_unref (old_val);

	return 1;
}

/**
 * Insert an element at the given position in the list #xmmsv_t.
 * The list will hold a reference to the element until it's removed.
 *
 * @param listv A #xmmsv_t containing a list.
 * @param pos The position in the list. If negative, start counting
 *            from the end (-1 is the last element, etc).
 * @param val The element to insert.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_insert (xmmsv_t *listv, int pos, xmmsv_t *val)
{
	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_list (listv), 0);
	x_return_val_if_fail (val, 0);

	return _xmmsv_list_insert (listv->value.list, pos, val);
}

/**
 * Remove the element at the given position from the list #xmmsv_t.
 *
 * @param listv A #xmmsv_t containing a list.
 * @param pos The position in the list. If negative, start counting
 *            from the end (-1 is the last element, etc).
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_remove (xmmsv_t *listv, int pos)
{
	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_list (listv), 0);

	return _xmmsv_list_remove (listv->value.list, pos);
}

/**
 * Append an element to the end of the list #xmmsv_t.
 * The list will hold a reference to the element until it's removed.
 *
 * @param listv A #xmmsv_t containing a list.
 * @param val The element to append.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_append (xmmsv_t *listv, xmmsv_t *val)
{
	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_list (listv), 0);
	x_return_val_if_fail (val, 0);

	return _xmmsv_list_append (listv->value.list, val);
}

/**
 * Empty the list from all its elements.
 *
 * @param listv A #xmmsv_t containing a list.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_clear (xmmsv_t *listv)
{
	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_list (listv), 0);

	_xmmsv_list_clear (listv->value.list);

	return 1;
}

/**
 * Apply a function to each element in the list, in sequential order.
 *
 * @param listv A #xmmsv_t containing a list.
 * @param function The function to apply to each element.
 * @param user_data User data passed to the foreach function.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_foreach (xmmsv_t *listv, xmmsv_list_foreach_func func,
                    void* user_data)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *v;

	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_list (listv), 0);
	x_return_val_if_fail (xmmsv_get_list_iter (listv, &it), 0);

	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &v);
		func (v, user_data);
		xmmsv_list_iter_next (it);
	}

	xmmsv_list_iter_free (it);

	return 1;
}

/**
 * Return the size of the list.
 *
 * @param listv The #xmmsv_t containing the list.
 * @return The size of the list, or -1 if listv is invalid.
 */
int
xmmsv_list_get_size (xmmsv_t *listv)
{
	x_return_val_if_fail (listv, -1);
	x_return_val_if_fail (xmmsv_is_list (listv), -1);

	return listv->value.list->size;
}


static xmmsv_list_iter_t *
xmmsv_list_iter_new (xmmsv_list_t *l)
{
	xmmsv_list_iter_t *it;

	it = x_new0 (xmmsv_list_iter_t, 1);
	if (!it) {
		x_oom ();
		return NULL;
	}

	it->parent = l;
	it->position = 0;

	/* register iterator into parent */
	l->iterators = x_list_prepend (l->iterators, it);

	return it;
}

static void
xmmsv_list_iter_free (xmmsv_list_iter_t *it)
{
	/* unref iterator from list and free it */
	it->parent->iterators = x_list_remove (it->parent->iterators, it);
	free (it);
}


/**
 * Get the element currently pointed at by the iterator. This
 * function does not increase the refcount of the element, the
 * reference is still owned by the list.
 *
 * @param listv A #xmmsv_list_iter_t.
 * @param val Pointer set to a borrowed reference to the element
 *            pointed at by the iterator.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_iter_entry (xmmsv_list_iter_t *it, xmmsv_t **val)
{
	x_return_val_if_fail (xmmsv_list_iter_valid (it), 0);

	*val = it->parent->list[it->position];

	return 1;
}

/**
 * Check whether the iterator is valid and points to a valid element.
 *
 * @param listv A #xmmsv_list_iter_t.
 * @return 1 if the iterator is valid, 0 otherwise
 */
int
xmmsv_list_iter_valid (xmmsv_list_iter_t *it)
{
	return it && (it->position < it->parent->size);
}

/**
 * Rewind the iterator to the start of the list.
 *
 * @param listv A #xmmsv_list_iter_t.
 * @return 1 upon success otherwise 0
 */
void
xmmsv_list_iter_first (xmmsv_list_iter_t *it)
{
	x_return_if_fail (it);

	it->position = 0;
}

/**
 * Advance the iterator to the next element in the list.
 *
 * @param listv A #xmmsv_list_iter_t.
 * @return 1 upon success otherwise 0
 */
void
xmmsv_list_iter_next (xmmsv_list_iter_t *it)
{
	x_return_if_fail (it);

	if (it->position < it->parent->size) {
		it->position++;
	}
}

/**
 * Move the iterator to the n-th element in the list.
 *
 * @param listv A #xmmsv_list_iter_t.
 * @param pos The position in the list. If negative, start counting
 *            from the end (-1 is the last element, etc).
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_iter_goto (xmmsv_list_iter_t *it, int pos)
{
	unsigned int abspos;

	x_return_val_if_fail (it, 0);

	if (!get_absolute_position (pos, it->parent->size, &abspos)) {
		return 0;
	}
	it->position = abspos;

	return 1;
}

/**
 * Insert an element in the list at the position pointed at by the
 * iterator.
 *
 * @param listv A #xmmsv_list_iter_t.
 * @param val The element to insert.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_iter_insert (xmmsv_list_iter_t *it, xmmsv_t *val)
{
	x_return_val_if_fail (it, 0);
	x_return_val_if_fail (val, 0);

	return _xmmsv_list_insert (it->parent, it->position, val);
}

/**
 * Remove the element in the list at the position pointed at by the
 * iterator.
 *
 * @param listv A #xmmsv_list_iter_t.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_iter_remove (xmmsv_list_iter_t *it)
{
	x_return_val_if_fail (it, 0);

	return _xmmsv_list_remove (it->parent, it->position);
}


/* Dict stuff */

struct xmmsv_dict_St {
	/* dict implemented as a flat [key1, val1, key2, val2, ...] list */
	xmmsv_list_t *flatlist;
	x_list_t *iterators;
};

struct xmmsv_dict_iter_St {
	/* iterator of the contained flatlist */
	xmmsv_list_iter_t *lit;
	xmmsv_dict_t *parent;
};

static xmmsv_dict_t *
xmmsv_dict_new (void)
{
	xmmsv_dict_t *dict;

	dict = x_new0 (xmmsv_dict_t, 1);
	if (!dict) {
		x_oom ();
		return NULL;
	}

	dict->flatlist = xmmsv_list_new ();

	return dict;
}

static void
xmmsv_dict_free (xmmsv_dict_t *dict)
{
	xmmsv_dict_iter_t *it;

	/* free iterators */
	while (dict->iterators) {
		it = (xmmsv_dict_iter_t *) dict->iterators->data;
		xmmsv_dict_iter_free (it);
	}

	xmmsv_list_free (dict->flatlist);

	free (dict);
}

/**
 * Get the element corresponding to the given key in the dict #xmmsv_t
 * (if it exists).  This function does not increase the refcount of
 * the element, the reference is still owned by the dict.
 *
 * @param dictv A #xmmsv_t containing a dict.
 * @param key The key in the dict.
 * @param val Pointer set to a borrowed reference to the element
 *            corresponding to the given key in the dict.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_get (xmmsv_t *dictv, const char *key, xmmsv_t **val)
{
	xmmsv_dict_iter_t *it;
	int ret = 1;

	x_return_val_if_fail (key, 0);
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_dict (dictv), 0);
	x_return_val_if_fail (xmmsv_get_dict_iter (dictv, &it), 0);

	if (!xmmsv_dict_iter_seek (it, key)) {
		ret = 0;
	}

	/* If found, return value and success */
	if (ret && val) {
		xmmsv_dict_iter_pair (it, NULL, val);
	}

	xmmsv_dict_iter_free (it);

	return ret;
}

/**
 * Insert an element under the given key in the dict #xmmsv_t. If the
 * key already referenced an element, that element is unref'd and
 * replaced by the new one.
 *
 * @param dictv A #xmmsv_t containing a dict.
 * @param key The key in the dict.
 * @param val The new element to insert in the dict.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_insert (xmmsv_t *dictv, const char *key, xmmsv_t *val)
{
	xmmsv_dict_iter_t *it;
	int ret;

	x_return_val_if_fail (key, 0);
	x_return_val_if_fail (val, 0);
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_dict (dictv), 0);
	x_return_val_if_fail (xmmsv_get_dict_iter (dictv, &it), 0);

	/* if key already present, replace value */
	if (xmmsv_dict_iter_seek (it, key)) {
		ret = xmmsv_dict_iter_set (it, val);

	/* else, insert a new key-value pair */
	} else {
		xmmsv_t *keyval;

		keyval = xmmsv_new_string (key);

		ret = xmmsv_list_iter_insert (it->lit, keyval);
		if (ret) {
			xmmsv_list_iter_next (it->lit);
			ret = xmmsv_list_iter_insert (it->lit, val);
			if (!ret) {
				/* we added the key, but we couldn't add the value.
				 * we remove the key again to put the dictionary back
				 * in a consistent state.
				 */
				it->lit->position--;
				xmmsv_list_iter_remove (it->lit);
			}
		}
		xmmsv_unref (keyval);
	}

	xmmsv_dict_iter_free (it);

	return ret;
}

/**
 * Remove the element corresponding to a given key in the dict
 * #xmmsv_t (if it exists).
 *
 * @param dictv A #xmmsv_t containing a dict.
 * @param key The key in the dict.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_remove (xmmsv_t *dictv, const char *key)
{
	xmmsv_dict_iter_t *it;
	int ret = 1;

	x_return_val_if_fail (key, 0);
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_dict (dictv), 0);
	x_return_val_if_fail (xmmsv_get_dict_iter (dictv, &it), 0);

	if (!xmmsv_dict_iter_seek (it, key)) {
		ret = 0;
	} else {
		ret = xmmsv_list_iter_remove (it->lit) &&
		      xmmsv_list_iter_remove (it->lit);
		/* FIXME: cleanup if only the first fails */
	}

	xmmsv_dict_iter_free (it);

	return ret;
}

/**
 * Empty the dict of all its elements.
 *
 * @param dictv A #xmmsv_t containing a dict.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_clear (xmmsv_t *dictv)
{
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_dict (dictv), 0);

	_xmmsv_list_clear (dictv->value.dict->flatlist);

	return 1;
}

/**
 * Apply a function to each key-element pair in the list. No
 * particular order is assumed.
 *
 * @param dictv A #xmmsv_t containing a dict.
 * @param function The function to apply to each key-element pair.
 * @param user_data User data passed to the foreach function.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_foreach (xmmsv_t *dictv, xmmsv_dict_foreach_func func,
                    void *user_data)
{
	xmmsv_dict_iter_t *it;
	const char *key;
	xmmsv_t *v;

	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_dict (dictv), 0);
	x_return_val_if_fail (xmmsv_get_dict_iter (dictv, &it), 0);

	while (xmmsv_dict_iter_valid (it)) {
		xmmsv_dict_iter_pair (it, &key, &v);
		func (key, v, user_data);
		xmmsv_dict_iter_next (it);
	}

	xmmsv_dict_iter_free (it);

	return 1;
}

/**
 * Return the size of the dict.
 *
 * @param dictv The #xmmsv_t containing the dict.
 * @return The size of the dict, or -1 if dict is invalid.
 */
int
xmmsv_dict_get_size (xmmsv_t *dictv)
{
	x_return_val_if_fail (dictv, -1);
	x_return_val_if_fail (xmmsv_is_dict (dictv), -1);

	return dictv->value.dict->flatlist->size / 2;
}

static xmmsv_dict_iter_t *
xmmsv_dict_iter_new (xmmsv_dict_t *d)
{
	xmmsv_dict_iter_t *it;

	it = x_new0 (xmmsv_dict_iter_t, 1);
	if (!it) {
		x_oom ();
		return NULL;
	}

	it->lit = xmmsv_list_iter_new (d->flatlist);
	it->parent = d;

	/* register iterator into parent */
	d->iterators = x_list_prepend (d->iterators, it);

	return it;
}

static void
xmmsv_dict_iter_free (xmmsv_dict_iter_t *it)
{
	/* we don't free the parent list iter, already managed by the flatlist */

	/* unref iterator from dict and free it */
	it->parent->iterators = x_list_remove (it->parent->iterators, it);
	free (it);
}

/**
 * Get the key-element pair currently pointed at by the iterator. This
 * function does not increase the refcount of the element, the
 * reference is still owned by the dict.
 *
 * @param dictv A #xmmsv_dict_iter_t.
 * @param key Pointer set to the key pointed at by the iterator.
 * @param val Pointer set to a borrowed reference to the element
 *            pointed at by the iterator.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_iter_pair (xmmsv_dict_iter_t *it, const char **key,
                      xmmsv_t **val)
{
	unsigned int orig;
	xmmsv_t *v;

	if (!xmmsv_dict_iter_valid (it)) {
		return 0;
	}

	/* FIXME: avoid leaking abstraction! */
	orig = it->lit->position;

	if (key) {
		xmmsv_list_iter_entry (it->lit, &v);
		xmmsv_get_string (v, key);
	}

	if (val) {
		xmmsv_list_iter_next (it->lit);
		xmmsv_list_iter_entry (it->lit, val);
	}

	it->lit->position = orig;

	return 1;
}

/**
 * Check whether the iterator is valid and points to a valid pair.
 *
 * @param dictv A #xmmsv_dict_iter_t.
 * @return 1 if the iterator is valid, 0 otherwise
 */
int
xmmsv_dict_iter_valid (xmmsv_dict_iter_t *it)
{
	return it && xmmsv_list_iter_valid (it->lit);
}

/**
 * Rewind the iterator to the start of the dict.
 *
 * @param dictv A #xmmsv_dict_iter_t.
 * @return 1 upon success otherwise 0
 */
void
xmmsv_dict_iter_first (xmmsv_dict_iter_t *it)
{
	x_return_if_fail (it);

	xmmsv_list_iter_first (it->lit);
}

/**
 * Advance the iterator to the next pair in the dict.
 *
 * @param dictv A #xmmsv_dict_iter_t.
 * @return 1 upon success otherwise 0
 */
void
xmmsv_dict_iter_next (xmmsv_dict_iter_t *it)
{
	x_return_if_fail (it);

	/* skip a pair */
	xmmsv_list_iter_next (it->lit);
	xmmsv_list_iter_next (it->lit);
}

/**
 * Move the iterator to the pair with the given key (if it exists)
 * or move it to the position where the key would have to be
 * put (if it doesn't exist yet).
 *
 * @param dictv A #xmmsv_dict_iter_t.
 * @param key The key to seek for.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_iter_seek (xmmsv_dict_iter_t *it, const char *key)
{
	xmmsv_t *val;
	const char *k;
	int s, dict_size, cmp, left, right;

	x_return_val_if_fail (it, 0);
	x_return_val_if_fail (key, 0);

	/* how many key-value pairs does this dictionary contain? */
	dict_size = it->parent->flatlist->size / 2;

	/* if it's empty, point the iterator at the beginning of
	 * the list and report failure.
	 */
	if (!dict_size) {
		xmmsv_list_iter_goto (it->lit, 0);

		return 0;
	}

	/* perform binary search for the given key */
	left = 0;
	right = dict_size - 1;

	while (left <= right) {
		int mid = left + ((right - left) / 2);

		/* jump to the middle of the current search area */
		xmmsv_list_iter_goto (it->lit, mid * 2);
		xmmsv_list_iter_entry (it->lit, &val);

		/* get the key at this slot */
		s = xmmsv_get_string (val, &k);
		x_return_val_if_fail (s, 0);

		/* and compare it to the given key */
		cmp = strcmp (k, key);

		/* hooray, we found the key. */
		if (cmp == 0)
			return 1;

		/* go on searching the left or the right hand side. */
		if (cmp < 0) {
			left = mid + 1;
		} else {
			right = mid - 1;
		}
	}

	/* if we get down here, we failed to find the key
	 * in the dictionary.
	 * now, move the iterator so that it points to the slot
	 * where the key would be inserted.
	 */
	if (cmp < 0) {
		xmmsv_list_iter_next (it->lit);
		xmmsv_list_iter_next (it->lit);
	}

	return 0;
}

/**
 * Replace the element of the pair currently pointed to by the
 * iterator.
 *
 * @param dictv A #xmmsv_dict_iter_t.
 * @param val The element to set in the pair.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_iter_set (xmmsv_dict_iter_t *it, xmmsv_t *val)
{
	unsigned int orig;
	int ret;

	x_return_val_if_fail (xmmsv_dict_iter_valid (it), 0);

	/* FIXME: avoid leaking abstraction! */
	orig = it->lit->position;

	xmmsv_list_iter_next (it->lit);
	xmmsv_list_iter_remove (it->lit);
	ret = xmmsv_list_iter_insert (it->lit, val);
	/* FIXME: check remove success, swap operations? */

	it->lit->position = orig;

	return ret;
}

/**
 * Remove the pair in the dict pointed at by the iterator.
 *
 * @param dictv A #xmmsv_dict_iter_t.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_iter_remove (xmmsv_dict_iter_t *it)
{
	int ret = 0;

	ret = xmmsv_list_iter_remove (it->lit) &&
	      xmmsv_list_iter_remove (it->lit);
	/* FIXME: cleanup if only the first fails */

	return ret;
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
	char *url;
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

#include <stdarg.h>

xmmsv_t *
xmmsv_build_dict (const char *firstkey, ...)
{
	va_list ap;
	const char *key;
	xmmsv_t *val, *res;

	res = xmmsv_new_dict ();
	if (!res)
		return NULL;

	va_start (ap, firstkey);

	key = firstkey;
	do {
		val = va_arg (ap, xmmsv_t *);

		if (!xmmsv_dict_insert (res, key, val)) {
			xmmsv_unref (res);
			res = NULL;
			break;
		}
		key = va_arg (ap, const char *);
	} while (key);

	va_end (ap);

	return res;
}


/** @} */

static int
get_absolute_position (int pos, unsigned int size, unsigned int *abspos)
{
	int ret;

	if (pos >= 0 && pos <= size) {
		*abspos = pos;
		ret = 1;
	} else if (pos < 0 && -pos <= size) {
		*abspos = ((int) size) + pos; /* negative index, compute from end */
		ret = 1;
	} else {
		ret = 0; /* out of bounds index */
	}

	return ret;
}
