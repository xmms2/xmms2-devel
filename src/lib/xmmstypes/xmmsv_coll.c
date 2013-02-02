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
#include <string.h>
#include <ctype.h>

#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsv.h"
#include "xmmsc/xmmsv_coll.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmspriv/xmmsv.h"
#include "xmmsclientpriv/xmmsclient_util.h"

struct xmmsv_coll_internal_St {
	xmmsv_coll_type_t type;
	xmmsv_t *operands;
	xmmsv_t *attributes;
	xmmsv_t *idlist;
};

static xmmsv_coll_internal_t *_xmmsv_coll_new (xmmsv_coll_type_t type);


/**
 * @defgroup Collections Collections
 * @ingroup ValueType
 * @brief The API to be used to work with collection structures.
 * @{
 */

/**
 * Allocates a new collection #xmmsv_t of the given type.
 *
 * @param type the #xmmsv_coll_type_t specifying the type of collection to create.
 * @return The new #xmmsv_t. Must be unreferenced with #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_coll (xmmsv_coll_type_t type)
{
	xmmsv_coll_internal_t *coll;
	xmmsv_t *val;

	coll = _xmmsv_coll_new (type);
	if (coll == NULL) {
		return NULL;
	}

	val = _xmmsv_new (XMMSV_TYPE_COLL);
	if (val == NULL) {
		_xmmsv_coll_free (coll);
		return NULL;
	}

	val->value.coll = coll;

	return val;
}

xmmsv_t *
xmmsv_coll_new (xmmsv_coll_type_t type)
{
	return xmmsv_new_coll (type);
}

/**
 * Increases the references for the #xmmsv_coll_t
 *
 * @deprecated Use xmmsv_ref instead.
 *
 * @param coll the collection to reference.
 * @return coll
 */
xmmsv_coll_t *
xmmsv_coll_ref (xmmsv_coll_t *coll)
{
	return xmmsv_ref (coll);
}

/**
 * Decreases the references for the #xmmsv_coll_t
 * When the number of references reaches 0 it will
 * be freed and all its operands unreferenced as well.
 *
 * @deprecated Use xmmsv_unref instead.
 *
 * @param coll the collection to unref.
 */
void
xmmsv_coll_unref (xmmsv_coll_t *coll)
{
	xmmsv_unref (coll);
}

/**
 * Allocate a new collection of the given type.
 * The pointer will have to be deallocated using #xmmsv_coll_unref.
 *
 * @param type the #xmmsv_coll_type_t specifying the type of collection to create.
 * @return a pointer to the newly created collection, or NULL if the type is invalid.
 */
static xmmsv_coll_internal_t*
_xmmsv_coll_new (xmmsv_coll_type_t type)
{
	xmmsv_coll_internal_t *coll;

	x_return_val_if_fail (type <= XMMS_COLLECTION_TYPE_LAST, NULL);

	coll = x_new0 (xmmsv_coll_internal_t, 1);
	if (!coll) {
		x_oom ();
		return NULL;
	}

	coll->type = type;

	coll->idlist = xmmsv_new_list ();
	xmmsv_list_restrict_type (coll->idlist, XMMSV_TYPE_INT32);

	coll->operands = xmmsv_new_list ();
	xmmsv_list_restrict_type (coll->operands, XMMSV_TYPE_COLL);

	coll->attributes = xmmsv_new_dict ();

	return coll;
}

/**
 * Free the memory owned by the collection.
 * You probably want to use #xmmsv_coll_unref instead, which handles
 * reference counting.
 *
 * @param coll the collection to free.
 */
void
_xmmsv_coll_free (xmmsv_coll_internal_t *coll)
{
	x_return_if_fail (coll);

	/* Unref all the operands and attributes */
	xmmsv_unref (coll->operands);
	xmmsv_unref (coll->attributes);
	xmmsv_unref (coll->idlist);

	free (coll);
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

	xmmsv_list_clear (coll->value.coll->idlist);
	for (i = 0; ids[i]; i++) {
		xmmsv_list_append_int (coll->value.coll->idlist, ids[i]);
	}
}

static int
_xmmsv_coll_operand_find (xmmsv_list_iter_t *it, xmmsv_coll_t *op)
{
	xmmsv_t *v;

	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &v);
		if (v == op) {
			return 1;
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
	x_return_if_fail (coll);
	x_return_if_fail (op);

	/* we used to check if it already existed here before */
	if (!xmmsv_get_list_iter (coll->value.coll->operands, &it))
		return;

	if (_xmmsv_coll_operand_find (it, op)) {
		x_api_warning ("with an operand already in operand list");
		xmmsv_list_iter_explicit_destroy (it);
		return;
	}

	xmmsv_list_iter_explicit_destroy (it);

	xmmsv_list_append (coll->value.coll->operands, op);
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

	if (!xmmsv_get_list_iter (coll->value.coll->operands, &it))
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

	return xmmsv_list_append_int (coll->value.coll->idlist, id);
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

	return xmmsv_list_insert_int (coll->value.coll->idlist, index, id);
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

	return xmmsv_list_move (coll->value.coll->idlist, index, newindex);
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

	return xmmsv_list_remove (coll->value.coll->idlist, index);
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

	return xmmsv_list_clear (coll->value.coll->idlist);
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

	return xmmsv_list_get_int (coll->value.coll->idlist, index, val);
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

	return xmmsv_list_set_int (coll->value.coll->idlist, index, val);
}

/**
 * Get the size of the idlist.
 * @param coll  The collection to update.
 * @return  The size of the idlist.
 */
int
xmmsv_coll_idlist_get_size (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_list_get_size (coll->value.coll->idlist);
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

	return coll->value.coll->type;
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

	return coll->value.coll->idlist;
}

xmmsv_t *
xmmsv_coll_operands_get (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, NULL);

	return coll->value.coll->operands;
}

xmmsv_t *
xmmsv_coll_attributes_get (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, NULL);

	return coll->value.coll->attributes;
}

/**
 * Replace all attributes in the given collection.
 *
 * @param coll The collection in which to set the attribute.
 * @param attributes The new attributes.
 */
void
xmmsv_coll_attributes_set (xmmsv_coll_t *coll, xmmsv_t *attributes)
{
	xmmsv_t *old;

	x_return_if_fail (coll);
	x_return_if_fail (attributes);
	x_return_if_fail (xmmsv_is_type (attributes, XMMSV_TYPE_DICT));

	old = coll->value.coll->attributes;
	coll->value.coll->attributes = xmmsv_ref (attributes);
	xmmsv_unref (old);
}

/**
 * Set a string attribute in the given collection.
 *
 * @param coll The collection in which to set the attribute.
 * @param key  The name of the attribute to set.
 * @param value The value of the attribute.
 */
void
xmmsv_coll_attribute_set (xmmsv_coll_t *coll, const char *key, const char *value)
{
	xmmsv_coll_attribute_set_string (coll, key, value);
}

/**
 * Set a string attribute in the given collection.
 *
 * @param coll The collection in which to set the attribute.
 * @param key  The name of the attribute to set.
 * @param value The value of the attribute.
 */
void
xmmsv_coll_attribute_set_string (xmmsv_coll_t *coll, const char *key, const char *value)
{
	x_return_if_fail (xmmsv_is_type (coll, XMMSV_TYPE_COLL));
	xmmsv_dict_set_string (coll->value.coll->attributes, key, value);
}

/**
 * Set an integer attribute in the given collection.
 *
 * @param coll The collection in which to set the attribute.
 * @param key  The name of the attribute to set.
 * @param value The value of the attribute.
 */
void
xmmsv_coll_attribute_set_int (xmmsv_coll_t *coll, const char *key, int32_t value)
{
	x_return_if_fail (xmmsv_is_type (coll, XMMSV_TYPE_COLL));
	xmmsv_dict_set_int (coll->value.coll->attributes, key, value);
}

/**
 * Set an attribute in the given collection.
 *
 * @param coll The collection in which to set the attribute.
 * @param key  The name of the attribute to set.
 * @param value The value of the attribute.
 */
void
xmmsv_coll_attribute_set_value (xmmsv_coll_t *coll, const char *key, xmmsv_t *value)
{
	x_return_if_fail (xmmsv_is_type (coll, XMMSV_TYPE_COLL));
	xmmsv_dict_set (coll->value.coll->attributes, key, value);
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
	return xmmsv_dict_remove (coll->value.coll->attributes, key);
}

/**
 * Retrieve a string attribute from the given collection.
 *
 * @param coll The collection to retrieve the attribute from.
 * @param key  The name of the attribute.
 * @param value The value of the attribute if found (owned by the collection).
 * @return 1 if the attribute was found, 0 otherwise
 */
int
xmmsv_coll_attribute_get (xmmsv_coll_t *coll, const char *key, const char **value)
{
	return xmmsv_coll_attribute_get_string (coll, key, value);
}

/**
 * Retrieve a string attribute from the given collection.
 *
 * @param coll The collection to retrieve the attribute from.
 * @param key  The name of the attribute.
 * @param value The value of the attribute if found (owned by the collection).
 * @return 1 if the attribute was found, 0 otherwise
 */
int
xmmsv_coll_attribute_get_string (xmmsv_coll_t *coll, const char *key, const char **value)
{
	x_return_val_if_fail (xmmsv_is_type (coll, XMMSV_TYPE_COLL), 0);
	return xmmsv_dict_entry_get_string (coll->value.coll->attributes, key, value);
}

/**
 * Retrieve an integer attribute from the given collection.
 *
 * @param coll The collection to retrieve the attribute from.
 * @param key  The name of the attribute.
 * @param value The value of the attribute if found (owned by the collection).
 * @return 1 if the attribute was found, 0 otherwise
 */
int
xmmsv_coll_attribute_get_int (xmmsv_coll_t *coll, const char *key, int32_t *value)
{
	x_return_val_if_fail (xmmsv_is_type (coll, XMMSV_TYPE_COLL), 0);
	return xmmsv_dict_entry_get_int (coll->value.coll->attributes, key, value);
}

/**
 * Retrieve an attribute from the given collection.
 *
 * @param coll The collection to retrieve the attribute from.
 * @param key  The name of the attribute.
 * @param value The value of the attribute if found (owned by the collection).
 * @return 1 if the attribute was found, 0 otherwise
 */
int
xmmsv_coll_attribute_get_value (xmmsv_coll_t *coll, const char *key, xmmsv_t **value)
{
	x_return_val_if_fail (xmmsv_is_type (coll, XMMSV_TYPE_COLL), 0);
	return xmmsv_dict_get (coll->value.coll->attributes, key, value);
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
	return xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
}

static xmmsv_t *
xmmsv_coll_normalize_order_arguments (xmmsv_t *value)
{
	xmmsv_t *order;
	const char *key;

	if (value == NULL) {
		return NULL;
	}

	if (xmmsv_is_type (value, XMMSV_TYPE_DICT)) {
		return xmmsv_ref (value);
	}

	x_api_error_if (!xmmsv_get_string (value, &key),
	                "order entry must be string or dict", NULL);

	order = xmmsv_new_dict ();

	if (key[0] == '-') {
		xmmsv_dict_set_string (order, "direction", "DESC");
		key++;
	}

	if (strcmp (key, "random") == 0) {
		xmmsv_dict_set_string (order, "type", "random");
	} else if (strcmp (key, "id") == 0) {
		xmmsv_dict_set_string (order, "type", "id");
	} else {
		xmmsv_dict_set_string (order, "type", "value");
		xmmsv_dict_set_string (order, "field", key);
	}

	return order;
}


/**
 * Return a collection with an order-operator added.
 *
 * @param coll  the original collection
 * @param value an ordering string, optionally starting with "-" (for
 *              descending ordering), followed by a string "id", "random"
 *              or a key identifying a  property, such as "artist" or "album".
 *              Or it can be a dict containing the attributes to set.
 * @return coll with order-operators added
 */
xmmsv_coll_t *
xmmsv_coll_add_order_operator (xmmsv_coll_t *coll, xmmsv_t *value)
{
	value = xmmsv_coll_normalize_order_arguments (value);
	if (value != NULL) {
		xmmsv_coll_t *ordered;

		ordered = xmmsv_new_coll (XMMS_COLLECTION_TYPE_ORDER);
		xmmsv_coll_add_operand (ordered, coll);
		xmmsv_coll_attributes_set (ordered, value);

		xmmsv_unref (value);

		return ordered;
	}

	return xmmsv_ref (coll);
}

/**
 * Return a collection with several order-operators added.
 *
 * @param coll	the original collection
 * @param order list of ordering strings or dicts.
 *
 * @return	coll with order-operators added
 */
xmmsv_coll_t *
xmmsv_coll_add_order_operators (xmmsv_coll_t *coll, xmmsv_t *order)
{
	xmmsv_coll_t *current;
	xmmsv_list_iter_t *it;

	x_api_error_if (coll == NULL, "with a NULL coll", NULL);

	xmmsv_ref (coll);

	if (!order) {
		return coll;
	}

	x_api_error_if (!xmmsv_is_type (order, XMMSV_TYPE_LIST),
	                "with a non list order", coll);

	current = coll;

	xmmsv_get_list_iter (order, &it);
	xmmsv_list_iter_last (it);

	while (xmmsv_list_iter_valid (it)) {
		xmmsv_coll_t *ordered;
		xmmsv_t *value;

		xmmsv_list_iter_entry (it, &value);

		ordered = xmmsv_coll_add_order_operator (current, value);
		xmmsv_unref (current);

		current = ordered;
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
	xmmsv_coll_t *ret;
	char str[12];

	x_return_val_if_fail (lim_start >= 0 && lim_len >= 0, NULL);

	if (lim_start == 0 && lim_len == 0) {
		return xmmsv_ref (coll);
	}

	ret = xmmsv_new_coll (XMMS_COLLECTION_TYPE_LIMIT);
	xmmsv_coll_add_operand (ret, coll);

	if (lim_start != 0) {
		int count = snprintf (str, sizeof (str), "%i", lim_start);
		if (count > 0 && count < sizeof (str)) {
			xmmsv_coll_attribute_set (ret, "start", str);
		} else {
			x_api_warning ("could not set collection limit operator start");
		}
	}

	if (lim_len != 0) {
		int count = snprintf (str, sizeof (str), "%i", lim_len);
		if (count > 0 && count < sizeof (str)) {
			xmmsv_coll_attribute_set (ret, "length", str);
		} else {
			x_api_warning ("could not set collection limit operator length");
		}
	}

	return ret;
}

/** @} */
