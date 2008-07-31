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

#include "xmmsclient/xmmsclient.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmspriv/xmms_list.h"


struct xmmsv_coll_St {

	/* refcounting */
	int ref;

	xmmsv_coll_type_t type;

	x_list_t *operands;
	x_list_t *curr_op;

	/* Stack of curr_op pointers to save/restore */
	x_list_t *curr_stack;

	/* stored as (key1, val1, key2, val2, ...) */
	x_list_t *attributes;
	x_list_t *curr_att;

	/* List of ids, 0-terminated. */
	uint32_t *idlist;
	size_t idlist_size;
	size_t idlist_allocated;

};


static void xmmsv_coll_free (xmmsv_coll_t *coll);
static int free_udata (void *ptr, void *userdata);

static int xmmsv_coll_unref_udata (void *coll, void *userdata);
static int xmmsv_coll_idlist_resize (xmmsv_coll_t *coll, size_t newsize);


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
 * @return a pointer to the newly created collection.
 */
xmmsv_coll_t*
xmmsv_coll_new (xmmsv_coll_type_t type)
{
	xmmsv_coll_t *coll;

	coll = x_new0 (xmmsv_coll_t, 1);
	if (!coll) {
		x_oom ();
		return NULL;
	}

	if (!(coll->idlist = x_new0 (uint32_t, 1))) {
		x_oom ();
		return NULL;
	}
	coll->idlist_size = 1;
	coll->idlist_allocated = 1;

	coll->ref  = 0;
	coll->type = type;

	coll->operands   = NULL;
	coll->attributes = NULL;

	coll->curr_op = coll->operands;
	coll->curr_stack = NULL;

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
	x_list_foreach (coll->operands, xmmsv_coll_unref_udata, NULL);
	x_list_foreach (coll->attributes, free_udata, NULL);

	x_list_free (coll->operands);
	x_list_free (coll->attributes);
	x_list_free (coll->curr_stack);

	free (coll->idlist);

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
xmmsv_coll_set_idlist (xmmsv_coll_t *coll, unsigned int ids[])
{
	unsigned int i;
	unsigned int size = 0;

	x_return_if_fail (coll);

	while (ids[size] != 0) {
		++size;
	}
	++size;

	free (coll->idlist);
	if (!(coll->idlist = x_new0 (uint32_t, size))) {
		x_oom ();
		return;
	}

	for (i = 0; i < size; ++i) {
		coll->idlist[i] = ids[i];
	}

	coll->idlist_size = size;
	coll->idlist_allocated = size;
}


/**
 * Add the operand to the given collection.
 * @param coll  The collection to add the operand to.
 * @param op    The operand to add.
 */
void
xmmsv_coll_add_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op)
{
	x_return_if_fail (coll);
	x_return_if_fail (op);

	/* Already present, don't add twice! */
	if (x_list_index (coll->operands, op) != -1) {
		return;
	}

	xmmsv_coll_ref (op);

	coll->operands = x_list_append (coll->operands, op);
}

/**
 * Remove all the occurences of the operand in the given collection.
 * @param coll  The collection to remove the operand from.
 * @param op    The operand to remove.
 */
void
xmmsv_coll_remove_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op)
{
	x_list_t *entry;

	x_return_if_fail (coll);
	x_return_if_fail (op);

	/* Find the entry, abort if not in the list */
	entry = x_list_find (coll->operands, op);
	if (entry == NULL) {
		return;
	}

	coll->operands = x_list_delete_link (coll->operands, entry);

	xmmsv_coll_unref (op);
}


/**
 * Append a value to the idlist.
 * @param coll  The collection to update.
 * @param id    The id to append to the idlist.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_append (xmmsv_coll_t *coll, unsigned int id)
{
	x_return_val_if_fail (coll, 0);

	return xmmsv_coll_idlist_insert (coll, coll->idlist_size - 1, id);
}

/**
 * Insert a value at a given position in the idlist.
 * @param coll  The collection to update.
 * @param id    The id to insert in the idlist.
 * @param index The position at which to insert the value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_insert (xmmsv_coll_t *coll, unsigned int index, unsigned int id)
{
	int i;
	x_return_val_if_fail (coll, 0);

	if (index >= coll->idlist_size) {
		return 0;
	}

	/* We need more memory, reallocate */
	if (coll->idlist_size == coll->idlist_allocated) {
		int success;
		size_t double_size = coll->idlist_allocated * 2;
		success = xmmsv_coll_idlist_resize (coll, double_size);
		x_return_val_if_fail (success, 0);
	}

	for (i = coll->idlist_size; i > index; i--) {
		coll->idlist[i] = coll->idlist[i - 1];
	}

	coll->idlist[index] = id;
	coll->idlist_size++;

	return 1;
}

/**
 * Move a value of the idlist to a new position.
 * @param coll  The collection to update.
 * @param index The index of the value to move.
 * @param newindex The newindex to which to move the value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_move (xmmsv_coll_t *coll, unsigned int index, unsigned int newindex)
{
	int i;
	uint32_t tmp;

	x_return_val_if_fail (coll, 0);

	if ((index >= coll->idlist_size - 1) || (newindex >= coll->idlist_size - 1)) {
		return 0;
	}

	tmp = coll->idlist[index];
	if (index < newindex) {
		for (i = index; i < newindex; i++) {
			coll->idlist[i] = coll->idlist[i + 1];
		}
	}
	else if (index > newindex) {
		for (i = index; i > newindex; i--) {
			coll->idlist[i] = coll->idlist[i - 1];
		}
	}
	coll->idlist[newindex] = tmp;

	return 1;
}

/**
 * Remove the value at a given index from the idlist.
 * @param coll  The collection to update.
 * @param index The index at which to remove the value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_remove (xmmsv_coll_t *coll, unsigned int index)
{
	int i;
	size_t half_size;

	x_return_val_if_fail (coll, 0);

	if (index >= coll->idlist_size - 1) {
		return 0;
	}

	coll->idlist_size--;
	for (i = index; i < coll->idlist_size; i++) {
		coll->idlist[i] = coll->idlist[i + 1];
	}

	/* Reduce memory usage by two if possible */
	half_size = coll->idlist_allocated / 2;
	if (coll->idlist_size <= half_size) {
		xmmsv_coll_idlist_resize (coll, half_size);
	}

	return 1;
}

/**
 * Empties the idlist.
 * @param coll  The collection to update.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_clear (xmmsv_coll_t *coll)
{
	unsigned int empty[] = { 0 };

	x_return_val_if_fail (coll, 0);

	xmmsv_coll_set_idlist (coll, empty);

	return 1;
}

/**
 * Retrieves the value at the given position in the idlist.
 * @param coll  The collection to update.
 * @param index The position of the value to retrieve.
 * @param val   The pointer at which to store the found value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_get_index (xmmsv_coll_t *coll, unsigned int index, uint32_t *val)
{
	x_return_val_if_fail (coll, 0);

	if (index >= (coll->idlist_size - 1)) {
		return 0;
	}

	*val = coll->idlist[index];

	return 1;
}

/**
 * Sets the value at the given position in the idlist.
 * @param coll  The collection to update.
 * @param index The position of the value to set.
 * @param val   The new value.
 * @return  TRUE on success, false otherwise.
 */
int
xmmsv_coll_idlist_set_index (xmmsv_coll_t *coll, unsigned int index, uint32_t val)
{
	x_return_val_if_fail (coll, 0);

	if (index >= (coll->idlist_size - 1)) {
		return 0;
	}

	coll->idlist[index] = val;

	return 1;
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

	return coll->idlist_size - 1;
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
 * @param coll  The collection to consider.
 * @return The 0-terminated list of ids.
 */
uint32_t*
xmmsv_coll_get_idlist (xmmsv_coll_t *coll)
{
	x_return_null_if_fail (coll);

	return coll->idlist;
}


/**
 * Move the internal pointer of the operand list to the first operand.
 *
 * @param coll  The collection to consider.
 * @return 1 upon success, 0 otherwise.
 */
int
xmmsv_coll_operand_list_first (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	coll->curr_op = coll->operands;

	return 1;
}

/**
 * Checks if the internal pointer points to a valid operand of the list.
 *
 * @param coll  The collection to consider.
 * @return 1 if the current operand is valid, 0 otherwise.
 */
int
xmmsv_coll_operand_list_valid (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	return (coll->curr_op != NULL);
}

/**
 * Provide a reference to the current operand in the list by changing
 * the operand parameter to point to it. Note that the refcount of the
 * operand is not modified by this operation.
 * The function returns 1 if the entry was valid, 0 otherwise.
 *
 * @param coll  The collection to consider.
 * @param operand  The current operand in the list.
 * @return 1 upon success (valid entry), 0 otherwise.
 */
int
xmmsv_coll_operand_list_entry (xmmsv_coll_t *coll, xmmsv_coll_t **operand)
{
	x_return_val_if_fail (coll, 0);
	if (coll->curr_op == NULL) {
		return 0;
	}

	*operand = (xmmsv_coll_t *)coll->curr_op->data;

	return 1;
}

/**
 * Move forward the internal pointer of the operand list.
 *
 * @param coll  The collection to consider.
 * @return 1 upon success, 0 otherwise.
 */
int
xmmsv_coll_operand_list_next (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);
	if (coll->curr_op == NULL) {
		return 0;
	}

	coll->curr_op = coll->curr_op->next;

	return 1;
}


/**
 * Save the position of the operand iterator, to be restored later by
 * calling #xmmsv_coll_operand_list_restore.  The pointer is saved on
 * a stack, so it can be called any number of times, as long as it is
 * restored as many times.
 * Note that the iterator is not tested for consistency before being
 * saved!
 *
 * @param coll  The collection to consider.
 * @return 1 upon success, 0 otherwise.
 */
int
xmmsv_coll_operand_list_save (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	coll->curr_stack = x_list_prepend (coll->curr_stack, coll->curr_op);

	return 1;
}


/**
 * Restore the position of the operand iterator, previously saved by
 * calling #xmmsv_coll_operand_list_save.
 * Note that the iterator is not tested for consistency, so you better
 * be careful if the list of operands was manipulated since the
 * iterator was saved!
 *
 * @param coll  The collection to consider.
 * @return 1 upon success, 0 otherwise.
 */
int
xmmsv_coll_operand_list_restore (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);
	x_return_val_if_fail (coll->curr_stack, 0);

	/* Pop stack head and restore curr_op */
	coll->curr_op = x_list_nth_data (coll->curr_stack, 0);
	coll->curr_stack = x_list_delete_link (coll->curr_stack, coll->curr_stack);

	return 1;
}


/**
 * Remove all the operands.
 *
 * @param coll  The collection to consider.
 */
void
xmmsv_coll_operand_list_clear (xmmsv_coll_t *coll)
{
	xmmsv_coll_t *op;

	x_return_if_fail (coll);

	/* Unref all the operands sequentially. */
	while (coll->operands != NULL) {
		op = (xmmsv_coll_t *) coll->operands->data;
		coll->operands = x_list_delete_link (coll->operands, coll->operands);
		xmmsv_coll_unref (op);
	}

	coll->curr_op = NULL;
	coll->curr_stack = NULL;
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
	x_list_t *n;
	for (n = coll->attributes; n; n = x_list_next (n)) {
		const char *k = n->data;
		if (strcasecmp (k, key) == 0 && n->next) {
			/* found right key, update value */
			free (n->next->data);
			n->next->data = strdup (value);
			return;
		} else {
			/* skip data part of this entry */
			n = x_list_next (n);
		}
	}

	/* Key not found, insert the new pair */
	coll->attributes = x_list_append (coll->attributes, strdup (key));
	coll->attributes = x_list_append (coll->attributes, strdup (value));

	return;
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
	x_list_t *n;
	for (n = coll->attributes; n; n = x_list_next (n)) {
		char *k = n->data;
		if (strcasecmp (k, key) == 0 && n->next) {
			char *v = n->next->data;
			/* found right key, remove key and value */
			coll->attributes = x_list_delete_link (coll->attributes, n->next);
			coll->attributes = x_list_delete_link (coll->attributes, n);
			free (k);
			free (v);
			return 1;
		} else {
			/* skip data part of this entry */
			n = x_list_next (n);
		}
	}

	/* Key not found */
	return 0;
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
	x_list_t *n;
	for (n = coll->attributes; n; n = x_list_next (n)) {
		const char *k = n->data;
		if (strcasecmp (k, key) == 0 && n->next) {
			/* found right key, return value */
			if (value) {
				*value = (char*) n->next->data;
			}

			return 1;
		} else {
			/* skip data part of this entry */
			n = x_list_next (n);
		}
	}

	if (value) {
		*value = NULL;
	}

	return 0;
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
	x_list_t *n;
	for (n = coll->attributes; n; n = x_list_next (n)) {
		const char *val = NULL;
		if (n->next) {
			val = n->next->data;
		}
		func ((const char*)n->data, val, user_data);
		n = x_list_next (n); /* skip data part */
	}

	return;
}

void
xmmsv_coll_attribute_list_first (xmmsv_coll_t *coll)
{
	x_return_if_fail (coll);

	coll->curr_att = coll->attributes;
}

int
xmmsv_coll_attribute_list_valid (xmmsv_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	return !!coll->curr_att;
}

void
xmmsv_coll_attribute_list_entry (xmmsv_coll_t *coll, const char **k, const char **v)
{
	x_return_if_fail (coll);
	x_return_if_fail (coll->curr_att);
	x_return_if_fail (coll->curr_att->next);

	*k = coll->curr_att->data;
	*v = coll->curr_att->next->data;
}

void
xmmsv_coll_attribute_list_next (xmmsv_coll_t *coll)
{
	x_return_if_fail (coll);

	if (coll->curr_att && coll->curr_att->next && coll->curr_att->next->next) {
		coll->curr_att = coll->curr_att->next->next;
	} else {
		coll->curr_att = NULL;
	}
}

/**
 * Return a collection referencing the whole media library,
 * that is a reference to the "All Media" collection.
 * The returned structure must be unref'd using #xmmsv_coll_unref
 * after usage.
 *
 * @return a collection referring to the "All Media" collection.
 */
xmmsv_coll_t*
xmmsv_coll_universe ()
{
	xmmsv_coll_t *univ = xmmsv_coll_new (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set (univ, "reference", "All Media");
	/* FIXME: namespace? */

	return univ;
}

/** @} */



/** @internal */

/**
 * Utility version of the #xmmsv_coll_unref function, voidified and
 * with an extra userdata argument, to be used as a foreach
 * function.
 */
static int
xmmsv_coll_unref_udata (void *coll, void *userdata)
{
	xmmsv_coll_unref ((xmmsv_coll_t *)coll);
	return 1;
}

/**
 * Alternate version of the free() C function with an extra userdata
 * argument, to be used as a foreach function.
 */
static int
free_udata (void *ptr, void *userdata)
{
	free (ptr);
	return 1;
}


static int
xmmsv_coll_idlist_resize (xmmsv_coll_t *coll, size_t newsize)
{
	uint32_t *newmem;

	newmem = realloc (coll->idlist, newsize * sizeof (uint32_t));

	if (newmem == NULL) {
		x_oom ();
		return 0;
	}

	coll->idlist = newmem;
	coll->idlist_allocated = newsize;

	return 1;
}
