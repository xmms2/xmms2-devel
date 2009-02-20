/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsv.h"
#include "xmmsc/xmmsv_coll.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmspriv/xmms_list.h"


struct xmmsv_coll_St {

	/* refcounting */
	int ref;

	xmmsv_coll_type_t type;
	xmmsv_t *operands;
	xmmsv_t *attributes;
	xmmsv_t *idlist;

	int32_t *legacy_idlist;
};


static void xmmsv_coll_free (xmmsv_coll_t *coll);


/**
 * @defgroup CollectionStructure CollectionStructure
 * @ingroup Collections
 * @brief The API to be used to work with collection structures.
 *
 * @{
 */

/**
 * Increases the references for the #xmmsv_coll_t
 *
 * @param coll the collection to reference.
 * @return coll
 */
xmmsv_coll_t *
xmmsv_coll_ref (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, NULL);

	coll->ref++;

	return coll;
}

/**
 * Allocate a new collection of the given type.
 * The pointer will have to be deallocated using #xmmsv_coll_unref.
 *
 * @param type the #xmmsv_coll_type_t specifying the type of collection to create.
 * @return a pointer to the newly created collection, or NULL if the type is invalid.
 */
xmmsv_coll_t*
xmmsv_coll_new (xmmsv_coll_type_t type)
{
	xmmsv_coll_t *coll;

	x_return_val_if_fail (type <= XMMS_COLLECTION_TYPE_LAST, NULL);

	coll = x_new0 (xmmsv_coll_t, 1);
	if (!coll) {
		x_oom ();
		return NULL;
	}

	coll->ref  = 0;
	coll->type = type;

	coll->idlist = xmmsv_new_list ();
	xmmsv_list_restrict_type (coll->idlist, XMMSV_TYPE_INT32);

	coll->operands = xmmsv_new_list ();
	xmmsv_list_restrict_type (coll->operands, XMMSV_TYPE_COLL);

	coll->attributes = xmmsv_new_dict ();

	coll->legacy_idlist = NULL;

	/* user must give this back */
	xmmsv_coll_ref (coll);

	return coll;
}

/**
 * Free the memory owned by the collection.
 * You probably want to use #xmmsv_coll_unref instead, which handles
 * reference counting.
 *
 * @param coll the collection to free.
 */
static void
xmmsv_coll_free (xmmsv_coll_t *coll)
{
	x_return_if_fail (coll);

	/* Unref all the operands and attributes */
	xmmsv_unref (coll->operands);
	xmmsv_unref (coll->attributes);
	xmmsv_unref (coll->idlist);
	if (coll->legacy_idlist) {
		free (coll->legacy_idlist);
	}

	free (coll);
}

/**
 * Decreases the references for the #xmmsv_coll_t
 * When the number of references reaches 0 it will
 * be freed and all its operands unreferenced as well.
 *
 * @param coll the collection to unref.
 */
void
xmmsv_coll_unref (xmmsv_coll_t *coll)
{
	x_return_if_fail (coll);
	x_api_error_if (coll->ref < 1, "with a freed collection",);

	coll->ref--;
	if (coll->ref == 0) {
		xmmsv_coll_free (coll);
	}
}


/**
 * Set the list of ids in the given collection.
 * The list must be 0-terminated.
 * Note that the idlist is only relevant for idlist collections.
 *
 * @param coll the collection to modify.
 * @param ids  the 0-terminated list of ids to store in the collection.
 */
void
xmmsv_coll_set_idlist (xmmsv_coll_t *coll, int ids[])
{
	unsigned int i;

	xmmsv_list_clear (coll->idlist);
	for (i = 0; ids[i]; i++) {
		xmmsv_list_append_int (coll->idlist, ids[i]);
	}
}

static int
_xmmsv_coll_operand_find (xmmsv_list_iter_t *it, xmmsv_coll_t *op)
{
	xmmsv_coll_t *c;
	xmmsv_t *v;

	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &v);
		if (xmmsv_get_coll (v, &c)) {
			if (c == op) {
				return 1;
			}
		}
		xmmsv_list_iter_next (it);
	}
	return 0;
}

/**
 * Add the operand to the given collection.
 * @param coll  The collection to add the operand to.
 * @param op    The operand to add.
 */
void
xmmsv_coll_add_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *v;
	x_return_if_fail (coll);
	x_return_if_fail (op);

	/* we used to check if it already existed here before */
	if (!xmmsv_get_list_iter (coll->operands, &it))
		return;

	if (_xmmsv_coll_operand_find (it, op)) {
		x_api_warning ("with an operand already in operand list");
		xmmsv_list_iter_explicit_destroy (it);
		return;
	}

	xmmsv_list_iter_explicit_destroy (it);

	v = xmmsv_new_coll (op);
	x_return_if_fail (v);
	xmmsv_list_append (coll->operands, v);
	xmmsv_unref (v);
}

/**
 * Remove all the occurences of the operand in the given collection.
 * @param coll  The collection to remove the operand from.
 * @param op    The operand to remove.
 */
void
xmmsv_coll_remove_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op)
{
	xmmsv_list_iter_t *it;

	x_return_if_fail (coll);
	x_return_if_fail (op);

	if (!xmmsv_get_list_iter (coll->operands, &it))
		return;

	if (_xmmsv_coll_operand_find (it, op)) {
		xmmsv_list_iter_remove (it);
	} else {
		x_api_warning ("with an operand not in operand list");
	}
	xmmsv_list_iter_explicit_destroy (it);
}


/**
 * Append a value to the idlist.
 * @param coll  The collection to update.
 * @param id    The id to append to the idlist.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_append (xmmsv_coll_t *coll, int id)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_append_int (coll->idlist, id);
}

/**
 * Insert a value at a given position in the idlist.
 * @param coll  The collection to update.
 * @param id    The id to insert in the idlist.
 * @param index The position at which to insert the value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_insert (xmmsv_coll_t *coll, int index, int id)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_insert_int (coll->idlist, index, id);
}

/**
 * Move a value of the idlist to a new position.
 * @param coll  The collection to update.
 * @param index The index of the value to move.
 * @param newindex The newindex to which to move the value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_move (xmmsv_coll_t *coll, int index, int newindex)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_move (coll->idlist, index, newindex);
}

/**
 * Remove the value at a given index from the idlist.
 * @param coll  The collection to update.
 * @param index The index at which to remove the value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_remove (xmmsv_coll_t *coll, int index)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_remove (coll->idlist, index);
}

/**
 * Empties the idlist.
 * @param coll  The collection to update.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_clear (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_clear (coll->idlist);
}

/**
 * Retrieves the value at the given position in the idlist.
 * @param coll  The collection to update.
 * @param index The position of the value to retrieve.
 * @param val   The pointer at which to store the found value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_get_index (xmmsv_coll_t *coll, int index, int32_t *val)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_get_int (coll->idlist, index, val);
}

/**
 * Sets the value at the given position in the idlist.
 * @param coll  The collection to update.
 * @param index The position of the value to set.
 * @param val   The new value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_set_index (xmmsv_coll_t *coll, int index, int32_t val)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_set_int (coll->idlist, index, val);
}

/**
 * Get the size of the idlist.
 * @param coll  The collection to update.
 * @return  The size of the idlist.
 */
size_t
xmmsv_coll_idlist_get_size (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_get_size (coll->idlist);
}



/**
 * Return the type of the collection.
 * @param coll  The collection to consider.
 * @return The #xmmsv_coll_type_t of the collection, or -1 if invalid.
 */
xmmsv_coll_type_t
xmmsv_coll_get_type (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, -1);

	return coll->type;
}

/**
 * Return the list of ids stored in the collection.
 * The list is owned by the collection.
 * Note that this must not be confused with the content of the
 * collection, which must be queried using xmmsc_coll_query_ids!
 *
 * Also note that this function is deprecated (use xmmsv_coll_idlist_get
 * instead) and that changes to the returned array will be ignored. The array
 * is also not updated when the idlist is changed using the supplied functions.
 * Additionally every call to this function allocates a new array, so calling
 * it repetitively will be a performance penalty.
 *
 * @param coll  The collection to consider.
 * @return The 0-terminated list of ids.
 */
const int32_t*
xmmsv_coll_get_idlist (xmmsv_coll_t *coll)
{
	xmmsv_list_iter_t *it;
	unsigned int i;
	int32_t entry;

	x_return_null_if_fail (coll);

	/* free and allocate a new legacy list */
	if (coll->legacy_idlist) {
		free (coll->legacy_idlist);
	}
	coll->legacy_idlist = calloc (xmmsv_coll_idlist_get_size (coll) + 1,
	                              sizeof (int32_t));

	/* copy contents to legacy list */
	for (xmmsv_get_list_iter (coll->idlist, &it), i = 0;
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it), i++) {
		xmmsv_list_iter_entry_int (it, &entry);
		coll->legacy_idlist[i] = entry;
	}
	coll->legacy_idlist[i] = 0;

	return coll->legacy_idlist;
}

/**
 * Return the list of ids stored in the collection.
 * This function does not increase the refcount of the list, the reference is
 * still owned by the collection.
 *
 * Note that this must not be confused with the content of the collection,
 * which must be queried using xmmsc_coll_query_ids!
 *
 * @param coll  The collection to consider.
 * @return The 0-terminated list of ids.
 */
xmmsv_t *
xmmsv_coll_idlist_get (xmmsv_coll_t *coll)
{
	x_return_null_if_fail (coll);

	return coll->idlist;
}

xmmsv_t *
xmmsv_coll_operands_get (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, NULL);

	return coll->operands;
}

xmmsv_t *
xmmsv_coll_attributes_get (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, NULL);

	return coll->attributes;
}

/**
 * Set an attribute in the given collection.
 *
 * @param coll The collection in which to set the attribute.
 * @param key  The name of the attribute to set.
 * @param value The value of the attribute.
 */
void
xmmsv_coll_attribute_set (xmmsv_coll_t *coll, const char *key, const char *value)
{
	xmmsv_t *v;

	v = xmmsv_new_string (value);
	x_return_if_fail (v);

	xmmsv_dict_set (coll->attributes, key, v);
	xmmsv_unref (v);
}

/**
 * Remove an attribute from the given collection.
 * The return value indicated whether the attribute was found (and
 * removed)
 *
 * @param coll The collection to remove the attribute from.
 * @param key  The name of the attribute to remove.
 * @return 1 upon success, 0 otherwise
 */
int
xmmsv_coll_attribute_remove (xmmsv_coll_t *coll, const char *key)
{
	return xmmsv_dict_remove (coll->attributes, key);
}

/**
 * Retrieve the value of the attribute of the given collection.
 * The return value is 1 if the attribute was found and 0 otherwise.
 * The value of the attribute is owned by the collection and must not
 * be freed by the caller.
 *
 * @param coll The collection to retrieve the attribute from.
 * @param key  The name of the attribute.
 * @param value The value of the attribute if found (owned by the collection).
 * @return 1 if the attribute was found, 0 otherwise
 */
int
xmmsv_coll_attribute_get (xmmsv_coll_t *coll, const char *key, char **value)
{
	if (xmmsv_dict_entry_get_string (coll->attributes, key, value)) {
		return 1;
	}
	*value = NULL;
	return 0;
}



struct attr_fe_data {
	xmmsv_coll_attribute_foreach_func func;
	void *userdata;
};

static void
attr_fe_func (const char *key, xmmsv_t *val, void *user_data)
{
	struct attr_fe_data *d = user_data;
	const char *v;
	int r;

	r = xmmsv_get_string (val, &v);
	x_return_if_fail (r)

	d->func (key, v, d->userdata);
}
/**
 * Iterate over all key/value-pair of the collection attributes.
 *
 * Calls specified function for each key/value-pair of the attribute list.
 *
 * void function (const char *key, const char *value, void *user_data);
 *
 * @param coll the #xmmsv_coll_t.
 * @param func function that is called for each key/value-pair
 * @param user_data extra data passed to func
 */
void
xmmsv_coll_attribute_foreach (xmmsv_coll_t *coll,
                              xmmsv_coll_attribute_foreach_func func,
                              void *user_data)
{
	struct attr_fe_data d = {func, user_data};
	xmmsv_dict_foreach (coll->attributes, attr_fe_func, &d);
}

/**
 * Return a collection referencing the whole media library.
 * The returned structure must be unref'd using #xmmsv_coll_unref
 * after usage.
 *
 * @return a collection containing all media.
 */
xmmsv_coll_t*
xmmsv_coll_universe ()
{
	return xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
}

/**
 * Return a collection with an order-operator added.
 *
 * @param coll	the original collection
 * @param key	an ordering string, optionally starting with "-" (for descending
 * ordering), followed by a string "id", "random" or a key identifying a
 * property, such as "artist" or "album"
 *
 * @return	coll with order-operators added
 */
xmmsv_coll_t *
xmmsv_coll_add_order_operator (xmmsv_coll_t *coll, const char *key) {
	xmmsv_coll_t *new;

	new = xmmsv_coll_new (XMMS_COLLECTION_TYPE_ORDER);
	xmmsv_coll_add_operand (new, coll);

	if (key[0] == '-') {
		xmmsv_coll_attribute_set (new, "order", "DESC");
		key++;
	} else {
		xmmsv_coll_attribute_set (new, "order", "ASC");
	}

	if (strcmp (key, "random") == 0) {
		xmmsv_coll_attribute_set (new, "type", "random");
	} else if (strcmp (key, "id") == 0) {
		xmmsv_coll_attribute_set (new, "type", "id");
	} else {
		xmmsv_coll_attribute_set (new, "type", "value");
		xmmsv_coll_attribute_set (new, "field", key);
	}

	return new;
}

/**
 * Return a collection with several order-operators added.
 *
 * @param coll	the original collection
 * @param order	list of ordering strings
 *
 * @return	coll with order-operators added
 */
xmmsv_coll_t *
xmmsv_coll_add_order_operators (xmmsv_coll_t *coll, xmmsv_t *order)
{
	xmmsv_coll_t *new, *current;
	xmmsv_list_iter_t *it;
	xmmsv_t *val;
	const char *str;

	/* FIXME: check if order is a list */

	xmmsv_coll_ref (coll);

	if (!order) {
		return coll;
	}

	current = coll;
	xmmsv_get_list_iter (order, &it);
	xmmsv_list_iter_last (it);

	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &val);
		if (!xmmsv_get_string (val, &str)) {
			/* FIXME: how do we throw clientlib errors? */
		}

		new = xmmsv_coll_add_order_operator (current, str);
		xmmsv_coll_unref (current);

		current = new;
		xmmsv_list_iter_prev (it);
	}

	return current;
}

/**
 * Returns a collection with a LIMIT operator added
 *
 * @param coll The collection to add the limit operator to
 * @param lim_start The index of the first element to include, or 0 to disable
 * @param lim_len The length of the interval, or 0 to disable
 * @return A new collection with LIMIT operator added
 */
xmmsv_coll_t *
xmmsv_coll_add_limit_operator (xmmsv_coll_t *coll, int lim_start, int lim_len)
{
	xmmsv_coll_t *ret = xmmsv_coll_new (XMMS_COLLECTION_TYPE_LIMIT);
	char str[12];

	if (lim_start == 0 && lim_len == 0) {
		return xmmsv_coll_ref (coll);
	}

	xmmsv_coll_add_operand (ret, coll);

	if (lim_start != 0) {
		sprintf (str, "%i", lim_start);
		xmmsv_coll_attribute_set (ret, "start", str);
	}

	if (lim_len != 0) {
		sprintf (str, "%i", lim_len);
		xmmsv_coll_attribute_set (ret, "length", str);
	}

	return ret;
}

/** @} */
