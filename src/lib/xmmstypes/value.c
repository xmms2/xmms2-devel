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


static xmmsv_dict_t *xmmsv_dict_new (void);
static void xmmsv_dict_free (xmmsv_dict_t *dict);



static xmmsv_dict_iter_t *xmmsv_dict_iter_new (xmmsv_dict_t *d);
static void xmmsv_dict_iter_free (xmmsv_dict_iter_t *it);

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
			xmmsv_dict_free (val->value.dict);
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

/**
 * Gets the type of a dict entry.
 *
 * @param val A xmmsv_t containing a dict.
 * @param key The key in the dict.
 * @return The type of the entry or #XMMSV_TYPE_NONE if something goes wrong.
 */
xmmsv_type_t
xmmsv_dict_entry_get_type (xmmsv_t *val, const char *key)
{
	xmmsv_t *v;

	if (!xmmsv_dict_get (val, key, &v)) {
		return XMMSV_TYPE_NONE;
	}

	return xmmsv_get_type (v);
}


/* macro-magically define dict extractors */
#define GEN_DICT_EXTRACTOR_FUNC(typename, type)			\
	int								\
	xmmsv_dict_entry_get_##typename (xmmsv_t *val, const char *key, \
	                                 type *r)			\
	{								\
		xmmsv_t *v;						\
		if (!xmmsv_dict_get (val, key, &v)) {			\
			return 0;					\
		}							\
		return xmmsv_get_##typename (v, r);			\
	}

GEN_DICT_EXTRACTOR_FUNC (string, const char *)
GEN_DICT_EXTRACTOR_FUNC (int, int32_t)
GEN_DICT_EXTRACTOR_FUNC (coll, xmmsv_coll_t *)

/* macro-magically define dict set functions */
#define GEN_DICT_SET_FUNC(typename, type) \
	int \
	xmmsv_dict_set_##typename (xmmsv_t *dict, const char *key, type elem) \
	{ \
		int ret; \
		xmmsv_t *v; \
 \
		v = xmmsv_new_##typename (elem); \
		ret = xmmsv_dict_set (dict, key, v); \
		xmmsv_unref (v); \
 \
		return ret; \
	}

GEN_DICT_SET_FUNC (string, const char *)
GEN_DICT_SET_FUNC (int, int32_t)
GEN_DICT_SET_FUNC (coll, xmmsv_coll_t *)

/* macro-magically define dict_iter extractors */
#define GEN_DICT_ITER_EXTRACTOR_FUNC(typename, type) \
	int \
	xmmsv_dict_iter_pair_##typename (xmmsv_dict_iter_t *it, \
	                                 const char **key, \
	                                 type *r) \
	{ \
		xmmsv_t *v; \
		if (!xmmsv_dict_iter_pair (it, key, &v)) { \
			return 0; \
		} \
		if (r) { \
			return xmmsv_get_##typename (v, r); \
		} else { \
			return 1; \
		} \
	}

GEN_DICT_ITER_EXTRACTOR_FUNC (string, const char *)
GEN_DICT_ITER_EXTRACTOR_FUNC (int, int32_t)
GEN_DICT_ITER_EXTRACTOR_FUNC (coll, xmmsv_coll_t *)

/* macro-magically define dict_iter set functions */
#define GEN_DICT_ITER_SET_FUNC(typename, type) \
	int \
	xmmsv_dict_iter_set_##typename (xmmsv_dict_iter_t *it, type elem) \
	{ \
		int ret; \
		xmmsv_t *v; \
 \
		v = xmmsv_new_##typename (elem); \
		ret = xmmsv_dict_iter_set (it, v); \
		xmmsv_unref (v); \
 \
		return ret; \
	}

GEN_DICT_ITER_SET_FUNC (string, const char *)
GEN_DICT_ITER_SET_FUNC (int, int32_t)
GEN_DICT_ITER_SET_FUNC (coll, xmmsv_coll_t *)

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



/* Dict stuff */

typedef struct {
	uint32_t hash;
	char *str;
	xmmsv_t *value;
} xmmsv_dict_data_t;

struct xmmsv_dict_St {
	int elems;
	int size;
	xmmsv_dict_data_t *data;

	x_list_t *iterators;
};

struct xmmsv_dict_iter_St {
	int pos;
	xmmsv_dict_t *parent;
};

#define HASH_MASK(table) ((1 << (table)->size) - 1)
#define HASH_FILL_LIM 7
#define DELETED_STR ((char*)-1)
#define DICT_INIT_DATA(s) {.hash = xmmsv_dict_hash (s, strlen (s)), .str = (char*)s}
#define START_SIZE 2

/* MurmurHash2, by Austin Appleby */
static uint32_t
xmmsv_dict_hash (const void *key, int len)
{
	/* 'm' and 'r' are mixing constants generated offline.
	 * They're not really 'magic', they just happen to work well.
	 */
	const uint32_t seed = 0x12345678;
	const uint32_t m = 0x5bd1e995;
	const int r = 24;

	/* Initialize the hash to a 'random' value */
	uint32_t h = seed ^ len;

	/* Mix 4 bytes at a time into the hash */
	const unsigned char * data = (const unsigned char *)key;

	while (len >= 4)
	{
		uint32_t k;
		memcpy (&k, data, sizeof (k));

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	/* Handle the last few bytes of the input array */
	switch (len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	};

	/* Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated.
	 */
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}

/* Searches the hash table for an entry matching the hash and string in data.
 * It will save the found position in pos.
 * If a deleted position was found before the key, it will be saved in deleted
 * Returns 1 if the entry was found, 0 otherwise
 */
static int
xmmsv_dict_search (xmmsv_dict_t *dict, xmmsv_dict_data_t data, int *pos, int *deleted)
{
	int bucket = data.hash & HASH_MASK (dict);
	int stop = bucket;
	int size = 1 << dict->size;

	*deleted = -1;

	while (dict->data[bucket].str != NULL) {
		/* If this is a free entry we save it in the free pointer */
		if (dict->data[bucket].str == DELETED_STR) {
		   if (*deleted == -1) {
			   *deleted = bucket;
		   }
		/* If we found the entry we save it in the pos pointer */
		} else if (dict->data[bucket].hash == data.hash
		           && strcmp (dict->data[bucket].str, data.str) == 0) {
			*pos = bucket;
			return 1;
		}

		/* If we hit the end we roll around */
		if (++bucket >= size)
			bucket = 0;
		/* If we have checked the whole table we exit */
		if (bucket == stop)
			break;
	}

	/* Save the position to the first free entry
	 * (or the start bucket if we made it the whole way around
	 * without finding the entry and without finding any empty
	 * entries, but in that case free is set, since there have to
	 * be atleast 1 deleted or empty entry in the table)
	 */
	*pos = bucket;
	return 0;
}

/* Inserts data into the hash table */
static void
xmmsv_dict_insert (xmmsv_dict_t *dict, xmmsv_dict_data_t data, int alloc)
{
	int pos, deleted;

	if (xmmsv_dict_search (dict, data, &pos, &deleted)) {
		/* If the key already exists we change the data*/
		xmmsv_unref (dict->data[pos].value);
		dict->data[pos].value = data.value;
	} else {
		/* Otherwise we insert a new entry */
		if (alloc)
			data.str = strdup (data.str);
		dict->elems++;
		/* If we found a deleted entry before an empty one we use the free entry */
		if (deleted != -1) {
			dict->data[deleted] = data;
		} else {
			dict->data[pos] = data;
		}
	}
}

/* Remove an entry at the given position
 */
static void
xmmsv_dict_remove_internal (xmmsv_dict_t *dict, int pos)
{
	free ((void*)dict->data[pos].str);
	dict->data[pos].str = DELETED_STR;
	xmmsv_unref (dict->data[pos].value);
	dict->data[pos].value = NULL;
	dict->elems--;
}

/* Resizes the hash table by creating a new data table
 * twice the size of the old one
 */
static void
xmmsv_dict_resize (xmmsv_dict_t *dict)
{
	int i;
	xmmsv_dict_data_t *old_data;

	/* Double the table size */
	dict->size++;
	dict->elems = 0;
	old_data = dict->data;
	dict->data = x_new0 (xmmsv_dict_data_t, 1 << dict->size);

	/* Insert all the entries in the old table into the new one */
	for (i = 0; i < (1 << (dict->size - 1)); i++) {
		if (old_data[i].str != NULL) {
			xmmsv_dict_insert (dict, old_data[i], 0);
		}
	}

	free (old_data);
}

static xmmsv_dict_t *
xmmsv_dict_new (void)
{
	xmmsv_dict_t *dict;

	dict = x_new0 (xmmsv_dict_t, 1);
	if (!dict) {
		x_oom ();
		return NULL;
	}

	dict->size = 2;
	dict->data = x_new0 (xmmsv_dict_data_t, (1 << dict->size));

	if (!dict->data) {
		x_oom ();
		free (dict);
		return NULL;
	}

	return dict;
}

static void
xmmsv_dict_free (xmmsv_dict_t *dict)
{
	xmmsv_dict_iter_t *it;
	int i;

	/* free iterators */
	while (dict->iterators) {
		it = (xmmsv_dict_iter_t *) dict->iterators->data;
		xmmsv_dict_iter_free (it);
	}

	for (i = (1 << dict->size) - 1; i >= 0; i--) {
		if (dict->data[i].str != NULL) {
			if (dict->data[i].str != DELETED_STR) {
				free (dict->data[i].str);
				xmmsv_unref (dict->data[i].value);
			}
			dict->data[i].str = NULL;
		}
	}
	free (dict->data);
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
	xmmsv_dict_t *dict;
	int ret = 0;
	int pos, deleted;

	x_return_val_if_fail (key, 0);
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_type (dictv, XMMSV_TYPE_DICT), 0);

	xmmsv_dict_data_t data = DICT_INIT_DATA (key);
	dict = dictv->value.dict;

	if (xmmsv_dict_search (dict, data, &pos, &deleted)) {
		/* If there was a deleted entry before the one we found
		 * we can optimize a little by moving the entry to the
		 * deleted slot (and thus closer to the actual bucket it
		 * belongs to)
		 */
		if (deleted != -1) {
			dict->data[deleted] = dict->data[pos];
			dict->data[pos].str = DELETED_STR;
		}
		if (val != NULL) {
			*val = dict->data[pos].value;
		}
		ret = 1;
	}

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
xmmsv_dict_set (xmmsv_t *dictv, const char *key, xmmsv_t *val)
{
	xmmsv_dict_t *dict;
	int ret = 1;

	x_return_val_if_fail (key, 0);
	x_return_val_if_fail (val, 0);
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_type (dictv, XMMSV_TYPE_DICT), 0);

	xmmsv_dict_data_t data = DICT_INIT_DATA (key);
	data.value = xmmsv_ref (val);
	dict = dictv->value.dict;

	/* Resize if fill is too high */
	if (((dict->elems * 10) >> dict->size) > HASH_FILL_LIM) {
		xmmsv_dict_resize (dict);
	}

	xmmsv_dict_insert (dict, data, 1);

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
	xmmsv_dict_t *dict;
	int pos, deleted;
	int ret = 0;

	x_return_val_if_fail (key, 0);
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_type (dictv, XMMSV_TYPE_DICT), 0);

	xmmsv_dict_data_t data = DICT_INIT_DATA (key);
	dict = dictv->value.dict;

	/* If we find the entry we free the string and mark it as deleted */
	if (xmmsv_dict_search (dict, data, &pos, &deleted)) {
		xmmsv_dict_remove_internal (dict, pos);
		ret = 1;
	}

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
	int i;
	xmmsv_dict_t *dict;

	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_type (dictv, XMMSV_TYPE_DICT), 0);

	dict = dictv->value.dict;

	for (i = (1 << dict->size) - 1; i >= 0; i--) {
		if (dict->data[i].str != NULL) {
			if (dict->data[i].str != DELETED_STR) {
				free (dict->data[i].str);
				xmmsv_unref (dict->data[i].value);
			}
			dict->data[i].str = NULL;
		}
	}

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
	x_return_val_if_fail (xmmsv_is_type (dictv, XMMSV_TYPE_DICT), 0);
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
	x_return_val_if_fail (xmmsv_is_type (dictv, XMMSV_TYPE_DICT), -1);

	return dictv->value.dict->elems;
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

	it->parent = d;
	xmmsv_dict_iter_first (it);

	/* register iterator into parent */
	d->iterators = x_list_prepend (d->iterators, it);

	return it;
}

static void
xmmsv_dict_iter_free (xmmsv_dict_iter_t *it)
{
	/* unref iterator from dict and free it */
	it->parent->iterators = x_list_remove (it->parent->iterators, it);
	free (it);
}

/**
 * Explicitly free dict iterator.
 *
 * Immediately frees any resources used by this iterator. The iterator
 * is freed automatically when the dict is freed, but this function is
 * useful when the dict can be long lived.
 *
 * @param it iterator to free
 *
 */
void
xmmsv_dict_iter_explicit_destroy (xmmsv_dict_iter_t *it)
{
	xmmsv_dict_iter_free (it);
}

/**
 * Get the key-element pair currently pointed at by the iterator. This
 * function does not increase the refcount of the element, the
 * reference is still owned by the dict.
 *
 * @param it A #xmmsv_dict_iter_t.
 * @param key Pointer set to the key pointed at by the iterator.
 * @param val Pointer set to a borrowed reference to the element
 *            pointed at by the iterator.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_iter_pair (xmmsv_dict_iter_t *it, const char **key,
                      xmmsv_t **val)
{
	if (!xmmsv_dict_iter_valid (it)) {
		return 0;
	}

	if (key) {
		*key = it->parent->data[it->pos].str;
	}

	if (val) {
		*val = it->parent->data[it->pos].value;
	}

	return 1;
}

/**
 * Check whether the iterator is valid and points to a valid pair.
 *
 * @param it A #xmmsv_dict_iter_t.
 * @return 1 if the iterator is valid, 0 otherwise
 */
int
xmmsv_dict_iter_valid (xmmsv_dict_iter_t *it)
{
	return it && (it->pos < (1 << it->parent->size))
		&& it->parent->data[it->pos].str != NULL
		&& it->parent->data[it->pos].str != DELETED_STR;
}

/**
 * Rewind the iterator to the start of the dict.
 *
 * @param it A #xmmsv_dict_iter_t.
 * @return 1 upon success otherwise 0
 */
void
xmmsv_dict_iter_first (xmmsv_dict_iter_t *it)
{
	x_return_if_fail (it);
	xmmsv_dict_t *d = it->parent;

	for (it->pos = 0
	     ; it->pos < (1 << d->size) && (d->data[it->pos].str == NULL || d->data[it->pos].str == DELETED_STR)
	     ; it->pos++);
}

/**
 * Advance the iterator to the next pair in the dict.
 *
 * @param it A #xmmsv_dict_iter_t.
 * @return 1 upon success otherwise 0
 */
void
xmmsv_dict_iter_next (xmmsv_dict_iter_t *it)
{
	x_return_if_fail (it);
	xmmsv_dict_t *d = it->parent;

	for (it->pos++
	     ; it->pos < (1 << d->size) && (d->data[it->pos].str == NULL || d->data[it->pos].str == DELETED_STR)
	     ; it->pos++);
}

/**
 * Move the iterator to the pair with the given key (if it exists)
 * or move it to the position where the key would have to be
 * put (if it doesn't exist yet).
 *
 * @param it A #xmmsv_dict_iter_t.
 * @param key The key to seek for.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_iter_find (xmmsv_dict_iter_t *it, const char *key)
{
	x_return_val_if_fail (xmmsv_dict_iter_valid (it), 0);

	xmmsv_dict_iter_first (it);

	for (xmmsv_dict_iter_first (it)
	     ; xmmsv_dict_iter_valid (it)
	     ; xmmsv_dict_iter_next (it)) {
		const char *s;

		xmmsv_dict_iter_pair (it, &s, NULL);
		if (strcmp (s, key) == 0)
			return 1;
	}

	return 0;
}

/**
 * Replace the element of the pair currently pointed to by the
 * iterator.
 *
 * @param it A #xmmsv_dict_iter_t.
 * @param val The element to set in the pair.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_iter_set (xmmsv_dict_iter_t *it, xmmsv_t *val)
{
	x_return_val_if_fail (xmmsv_dict_iter_valid (it), 0);
	x_return_val_if_fail (val, 0);

	/* In case old value is new value, ref first. */
	xmmsv_ref (val);
	xmmsv_unref (it->parent->data[it->pos].value);
	it->parent->data[it->pos].value = val;

	return 1;
}

/**
 * Remove the pair in the dict pointed at by the iterator.
 *
 * @param it A #xmmsv_dict_iter_t.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_dict_iter_remove (xmmsv_dict_iter_t *it)
{
	x_return_val_if_fail (xmmsv_dict_iter_valid (it), 0);

	xmmsv_dict_remove_internal (it->parent, it->pos);
	xmmsv_dict_iter_next (it);

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



/**
 * @internal
 */

int
xmmsv_dict_has_key (xmmsv_t *dictv, const char *key)
{
	return xmmsv_dict_get (dictv, key, NULL);
}


/*
 *
 *
 */

/*
xmmsv_t *
xmmsv_serialize (xmmsv_t *val)
{
	switch (xmmsv_get_type (val)) {
	case XMMSV_TYPE_NONE:
		break;

	}
}
*/
