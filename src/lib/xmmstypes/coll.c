/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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


struct xmmsc_coll_St {

	/* refcounting */
	int ref;
 
	xmmsc_coll_type_t type;
 
	x_list_t *operands;
	x_list_t *curr_op;

	/* Stack of curr_op pointers to save/restore */
	x_list_t *curr_stack;
 
	/* stored as (key1, val1, key2, val2, ...) */
	x_list_t *attributes;
 
	/* List of ids, 0-terminated. */
	uint32_t *idlist;

};


static int xmmsc_coll_unref_udata (void *coll, void *userdata);


/**
 * @defgroup Collection Collection
 * @ingroup XMMSClient
 * @brief This performs everything related to collections.
 *
 * @{
 */

/**
 * Increases the references for the #xmmsc_coll_t
 *
 * @param coll the collection to reference.
 */
void 
xmmsc_coll_ref (xmmsc_coll_t *coll)
{
	x_return_if_fail (coll);
	coll->ref++;
}

/**
 * Allocate a new collection of the given type.
 * The pointer will have to be deallocated using #xmmsc_coll_unref.
 *
 * @param type the #xmmsc_coll_type_t specifying the type of collection to create.
 * @return a pointer to the newly created collection.
 */
xmmsc_coll_t*
xmmsc_coll_new (xmmsc_coll_type_t type)
{
	xmmsc_coll_t *coll;

	if (!(coll = x_new0 (xmmsc_coll_t, 1))) {
		x_oom();
		return NULL;
	}

	if (!(coll->idlist = x_new0 (uint32_t, 1))) {
		x_oom();
		return NULL;
	}

	coll->ref  = 0;
	coll->type = type;

	coll->operands   = NULL;
	coll->attributes = NULL;

	coll->curr_op = coll->operands;
	coll->curr_stack = NULL;

	/* user must give this back */
	xmmsc_coll_ref (coll);

	return coll;
}

/**
 * Free the memory owned by the collection.
 * You probably want to use #xmmsc_coll_unref instead, which handles
 * reference counting.
 *
 * @param coll the collection to free.
 */
void
xmmsc_coll_free (xmmsc_coll_t *coll)
{
	x_return_if_fail (coll);

	/* Unref all the operands */
	x_list_foreach (coll->operands, xmmsc_coll_unref_udata, NULL);

	x_list_free (coll->operands);
	x_list_free (coll->attributes);
	x_list_free (coll->curr_stack);

	free (coll->idlist);

	free (coll);
}

/**
 * Decreases the references for the #xmmsc_coll_t
 * When the number of references reaches 0 it will
 * be freed and all its operands unreferenced as well.
 *
 * @param coll the collection to unref.
 */
void 
xmmsc_coll_unref (xmmsc_coll_t *coll)
{
	x_return_if_fail (coll);
	x_api_error_if (coll->ref < 1, "with a freed collection",);

	coll->ref--;
	if (coll->ref == 0) {
		xmmsc_coll_free (coll);
	}
}


/**
 * Change the type of the given collection.
 * Note that internal properties of the collections are not flushed
 * even if they are not relevant to the new type.
 *
 * @param coll the collection to modify.
 * @param type the new type for the collection.
 */
void
xmmsc_coll_set_type (xmmsc_coll_t *coll, xmmsc_coll_type_t type)
{
	x_return_if_fail (coll);

	coll->type = type;
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
xmmsc_coll_set_idlist (xmmsc_coll_t *coll, unsigned int ids[])
{
	unsigned int i;
	unsigned int size = 0;

	x_return_if_fail (coll);

	while (ids[size] != 0) {
		++size;
	}

	free (coll->idlist);
	if (!(coll->idlist = x_new0 (uint32_t, size + 1))) {
		x_oom();
		return;
	}

	for (i = 0; i < size; ++i) {
		coll->idlist[i] = ids[i];
	}
}


/**
 * Add the operand to the given collection.
 * @param coll  The collection to add the operand to.
 * @param op    The operand to add.
 */
void
xmmsc_coll_add_operand (xmmsc_coll_t *coll, xmmsc_coll_t *op)
{
	x_return_if_fail (coll);
	x_return_if_fail (op);

	/* Already present, don't add twice! */
	if (x_list_index (coll->operands, op) != -1) {
		return;
	}

	xmmsc_coll_ref (op);

	coll->operands = x_list_append (coll->operands, op);
}

/**
 * Remove all the occurences of the operand in the given collection.
 * @param coll  The collection to remove the operand from.
 * @param op    The operand to remove.
 */
void
xmmsc_coll_remove_operand (xmmsc_coll_t *coll, xmmsc_coll_t *op)
{
	x_list_t *entry;

	x_return_if_fail (coll);
	x_return_if_fail (op);

	/* Find the entry, abort if not in the list */
	entry = x_list_find (coll->operands, op);
	if(entry == NULL) {
		return;
	}

	coll->operands = x_list_delete_link (coll->operands, entry);

	xmmsc_coll_unref (op);
}

 
/**
 * Return the type of the collection.
 * @param coll  The collection to consider.
 * @return The #xmmsc_coll_type_t of the collection, or -1 if invalid.
 */
xmmsc_coll_type_t
xmmsc_coll_get_type (xmmsc_coll_t *coll)
{
	x_return_val_if_fail (coll, -1);

	return coll->type;
}

/**
 * Return the list of ids stored in the collection.
 * The list is owned by the collection.
 * Note that this must not be confused with the content of the
 * collection, which must be queried using #xmmsc_coll_query_ids!
 *
 * @param coll  The collection to consider.
 * @return The 0-terminated list of ids.
 */
uint32_t*
xmmsc_coll_get_idlist (xmmsc_coll_t *coll)
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
xmmsc_coll_operand_list_first (xmmsc_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	coll->curr_op = coll->operands;

	return 1;
}

/**
 * Retrieve the current operand in the list
 * by changing the operand parameter to point to it.
 * The function returns 1 if the entry was valid, 0 otherwise.
 *
 * @param coll  The collection to consider.
 * @param operand  The current operand in the list.
 * @return 1 upon success (valid entry), 0 otherwise.
 */
int
xmmsc_coll_operand_list_entry (xmmsc_coll_t *coll, xmmsc_coll_t **operand)
{
	x_return_val_if_fail (coll, 0);
	if (coll->curr_op == NULL) {
		return 0;
	}

	*operand = (xmmsc_coll_t *)coll->curr_op->data;

	return 1;
}

/**
 * Move forward the internal pointer of the operand list.
 *
 * @param coll  The collection to consider.
 * @return 1 upon success, 0 otherwise.
 */
int
xmmsc_coll_operand_list_next (xmmsc_coll_t *coll)
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
 * calling #xmmsc_coll_operand_list_restore.  The pointer is saved on
 * a stack, so it can be called any number of times, as long as it is
 * restored as many times.
 * Note that the iterator is not tested for consistency before being
 * saved!
 *
 * @param coll  The collection to consider.
 * @return 1 upon success, 0 otherwise.
 */
int
xmmsc_coll_operand_list_save (xmmsc_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);

	coll->curr_stack = x_list_prepend (coll->curr_stack, coll->curr_op);

	return 1;
}


/**
 * Restore the position of the operand iterator, previously saved by
 * calling #xmmsc_coll_operand_list_save.
 * Note that the iterator is not tested for consistency, so you better
 * be careful if the list of operands was manipulated since the
 * iterator was saved!
 *
 * @param coll  The collection to consider.
 * @return 1 upon success, 0 otherwise.
 */
int
xmmsc_coll_operand_list_restore (xmmsc_coll_t *coll)
{
	x_return_val_if_fail (coll, 0);
	x_return_val_if_fail (coll->curr_stack, 0);

	/* Pop stack head and restore curr_op */
	coll->curr_op = x_list_nth_data (coll->curr_stack, 0);
	coll->curr_stack = x_list_delete_link (coll->curr_stack, coll->curr_stack);

	return 1;
}


/**
 * Set an attribute in the given collection.
 *
 * @param coll The collection in which to set the attribute.
 * @param key  The name of the attribute to set.
 * @param value The value of the attribute.
 */
void
xmmsc_coll_attribute_set (xmmsc_coll_t *coll, const char *key, const char *value)
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
xmmsc_coll_attribute_remove (xmmsc_coll_t *coll, const char *key)
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
 * The return value indicated whether the attribute was found (or NULL
 * if not found).  The value is owned by the collection.
 *
 * @param coll The collection to retrieve the attribute from.
 * @param key  The name of the attribute.
 * @param value The value of the attribute if found (owned by the collection).
 * @return 1 upon success, 0 otherwise
 */
int
xmmsc_coll_attribute_get (xmmsc_coll_t *coll, const char *key, char **value)
{
	x_list_t *n;
	for (n = coll->attributes; n; n = x_list_next (n)) {
		const char *k = n->data;
		if (strcasecmp (k, key) == 0 && n->next) {
			/* found right key, return value */
			*value = (char*) n->next->data;
			return 1;
		} else {
			/* skip data part of this entry */
			n = x_list_next (n);
		}
	}

	*value = NULL;
	return 0;
}

/**
 * Iterate over all key/value-pair of the collection attributes.
 *
 * Calls specified function for each key/value-pair of the attribute list.
 * 
 * void function (const char *key, const char *value, void *user_data);
 *
 * @param coll the #xmmsc_coll_t.
 * @param func function that is called for each key/value-pair
 * @param user_data extra data passed to func
 */
void
xmmsc_coll_attribute_foreach (xmmsc_coll_t *coll,
                              xmmsc_coll_attribute_foreach_func func,
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


/**
 * Return a collection referencing the whole media library,
 * i.e. the "All Media" collection.
 *
 * @return a collection refering the "All Media" collection.
 */
xmmsc_coll_t*
xmmsc_coll_universe ()
{
	xmmsc_coll_t *univ = xmmsc_coll_new (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsc_coll_attribute_set (univ, "reference", "All Media");
	/* FIXME: namespace? */

	return univ;
}

/** @} */



/** @internal */

/**
 * Utility version of the #xmmsc_coll_unref function, voidified and
 * with an extra userdata argument, to be used as a foreach
 * function.
 */
static int
xmmsc_coll_unref_udata (void *coll, void *userdata)
{
	xmmsc_coll_unref ((xmmsc_coll_t *)coll);
	return 1;
}

