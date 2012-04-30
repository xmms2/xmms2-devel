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

#include <stdlib.h>
#include <string.h>

#include "xmmspriv/xmmsv.h"
#include "xmmspriv/xmms_list.h"

typedef struct xmmsv_dict_data_St {
	uint32_t hash;
	char *str;
	xmmsv_t *value;
} xmmsv_dict_data_t;

struct xmmsv_dict_internal_St {
	int elems;
	int size;
	xmmsv_dict_data_t *data;

	x_list_t *iterators;
};

struct xmmsv_dict_iter_St {
	int pos;
	xmmsv_dict_internal_t *parent;
};

static void _xmmsv_dict_iter_free (xmmsv_dict_iter_t *it);

#define HASH_MASK(table) ((1 << (table)->size) - 1)
#define HASH_FILL_LIM 7
#define DELETED_STR ((char*)-1)
#define DICT_INIT_DATA(s) {.hash = _xmmsv_dict_hash (s, strlen (s)), .str = (char*)s}
#define START_SIZE 2

/* MurmurHash2, by Austin Appleby */
static uint32_t
_xmmsv_dict_hash (const void *key, int len)
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
_xmmsv_dict_search (xmmsv_dict_internal_t *dict, xmmsv_dict_data_t data,
                    int *pos, int *deleted)
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
_xmmsv_dict_insert (xmmsv_dict_internal_t *dict, xmmsv_dict_data_t data, int alloc)
{
	int pos, deleted;

	if (_xmmsv_dict_search (dict, data, &pos, &deleted)) {
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
_xmmsv_dict_remove (xmmsv_dict_internal_t *dict, int pos)
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
_xmmsv_dict_resize (xmmsv_dict_internal_t *dict)
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
			_xmmsv_dict_insert (dict, old_data[i], 0);
		}
	}

	free (old_data);
}

static xmmsv_dict_internal_t *
_xmmsv_dict_new (void)
{
	xmmsv_dict_internal_t *dict;

	dict = x_new0 (xmmsv_dict_internal_t, 1);
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

void
_xmmsv_dict_free (xmmsv_dict_internal_t *dict)
{
	xmmsv_dict_iter_t *it;
	int i;

	/* free iterators */
	while (dict->iterators) {
		it = (xmmsv_dict_iter_t *) dict->iterators->data;
		_xmmsv_dict_iter_free (it);
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
 * Allocates a new dict #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_dict (void)
{
	xmmsv_t *val = _xmmsv_new (XMMSV_TYPE_DICT);

	if (val) {
		val->value.dict = _xmmsv_dict_new ();
	}

	return val;
}

int
xmmsv_dict_has_key (xmmsv_t *dictv, const char *key)
{
	return xmmsv_dict_get (dictv, key, NULL);
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
	xmmsv_dict_internal_t *dict;
	int ret = 0;
	int pos, deleted;

	x_return_val_if_fail (key, 0);
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_type (dictv, XMMSV_TYPE_DICT), 0);

	xmmsv_dict_data_t data = DICT_INIT_DATA (key);
	dict = dictv->value.dict;

	if (_xmmsv_dict_search (dict, data, &pos, &deleted)) {
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
	xmmsv_dict_internal_t *dict;
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
		_xmmsv_dict_resize (dict);
	}

	_xmmsv_dict_insert (dict, data, 1);

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
	xmmsv_dict_internal_t *dict;
	int pos, deleted;
	int ret = 0;

	x_return_val_if_fail (key, 0);
	x_return_val_if_fail (dictv, 0);
	x_return_val_if_fail (xmmsv_is_type (dictv, XMMSV_TYPE_DICT), 0);

	xmmsv_dict_data_t data = DICT_INIT_DATA (key);
	dict = dictv->value.dict;

	/* If we find the entry we free the string and mark it as deleted */
	if (_xmmsv_dict_search (dict, data, &pos, &deleted)) {
		_xmmsv_dict_remove (dict, pos);
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
	xmmsv_dict_internal_t *dict;

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

	_xmmsv_dict_iter_free (it);

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
_xmmsv_dict_iter_new (xmmsv_dict_internal_t *d)
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
_xmmsv_dict_iter_free (xmmsv_dict_iter_t *it)
{
	/* unref iterator from dict and free it */
	it->parent->iterators = x_list_remove (it->parent->iterators, it);
	free (it);
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

	new_it = _xmmsv_dict_iter_new (val->value.dict);
	if (!new_it) {
		*it = NULL;
		return 0;
	}

	*it = new_it;

	return 1;
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
	_xmmsv_dict_iter_free (it);
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
	xmmsv_dict_internal_t *d = it->parent;

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
	xmmsv_dict_internal_t *d = it->parent;

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

	_xmmsv_dict_remove (it->parent, it->pos);
	xmmsv_dict_iter_next (it);

	return 1;
}

/* macro-magically define dict extractors */
#define GEN_DICT_EXTRACTOR_FUNC(typename, type)	  \
	int \
	xmmsv_dict_entry_get_##typename (xmmsv_t *val, const char *key, \
	                                 type *r) \
	{ \
		xmmsv_t *v; \
		if (!xmmsv_dict_get (val, key, &v)) { \
			return 0; \
		} \
		return xmmsv_get_##typename (v, r); \
	}

GEN_DICT_EXTRACTOR_FUNC (string, const char *)
GEN_DICT_EXTRACTOR_FUNC (int, int32_t)
GEN_DICT_EXTRACTOR_FUNC (coll, xmmsv_coll_t *)

/* macro-magically define dict set functions */
#define GEN_DICT_SET_FUNC(typename, type)	  \
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
#define GEN_DICT_ITER_EXTRACTOR_FUNC(typename, type)	  \
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
#define GEN_DICT_ITER_SET_FUNC(typename, type)	  \
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
