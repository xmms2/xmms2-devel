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
#include <string.h>

#include <xmmscpriv/xmmsv.h>
#include <xmmscpriv/xmms_list.h>

#include <xmmsc/xmmsv.h>

struct xmmsv_list_iter_St {
	xmmsv_list_internal_t *parent;
	int position;
};

struct xmmsv_list_internal_St {
	xmmsv_t **list;
	xmmsv_t *parent_value;
	int size;
	int allocated;
	bool restricted;
	xmmsv_type_t restricttype;
	x_list_t *iterators;
};

static void _xmmsv_list_iter_free (xmmsv_list_iter_t *it);

static int
_xmmsv_list_position_normalize (int *pos, int size, int allow_append)
{
	x_return_val_if_fail (size >= 0, 0);

	if (*pos < 0) {
		if (-*pos > size)
			return 0;
		*pos = size + *pos;
	}

	if (*pos > size)
		return 0;

	if (!allow_append && *pos == size)
		return 0;

	return 1;
}

static xmmsv_list_internal_t *
_xmmsv_list_new (void)
{
	xmmsv_list_internal_t *list;

	list = x_new0 (xmmsv_list_internal_t, 1);
	if (!list) {
		x_oom ();
		return NULL;
	}

	/* list is all empty for now! */

	return list;
}

void
_xmmsv_list_free (xmmsv_list_internal_t *l)
{
	xmmsv_list_iter_t *it;
	int i;

	/* free iterators */
	while (l->iterators) {
		it = (xmmsv_list_iter_t *) l->iterators->data;
		_xmmsv_list_iter_free (it);
	}

	/* unref contents */
	for (i = 0; i < l->size; i++) {
		xmmsv_unref (l->list[i]);
	}

	free (l->list);
	free (l);
}

static int
_xmmsv_list_resize (xmmsv_list_internal_t *l, int newsize)
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
_xmmsv_list_insert (xmmsv_list_internal_t *l, int pos, xmmsv_t *val)
{
	xmmsv_list_iter_t *it;
	x_list_t *n;

	if (!_xmmsv_list_position_normalize (&pos, l->size, 1)) {
		return 0;
	}

	if (l->restricted) {
		x_return_val_if_fail (xmmsv_is_type (val, l->restricttype), 0);
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
		success = _xmmsv_list_resize (l, double_size);
		x_return_val_if_fail (success, 0);
	}

	/* move existing items out of the way */
	if (l->size > pos) {
		memmove (l->list + pos + 1, l->list + pos,
		         (l->size - pos) * sizeof (xmmsv_t *));
	}

	l->list[pos] = xmmsv_ref (val);
	l->size++;

	/* update iterators pos */
	for (n = l->iterators; n; n = n->next) {
		it = (xmmsv_list_iter_t *) n->data;
		if (it->position > pos) {
			it->position++;
		}
	}

	return 1;
}

static int
_xmmsv_list_append (xmmsv_list_internal_t *l, xmmsv_t *val)
{
	return _xmmsv_list_insert (l, l->size, val);
}

static int
_xmmsv_list_remove (xmmsv_list_internal_t *l, int pos)
{
	xmmsv_list_iter_t *it;
	int half_size;
	x_list_t *n;

	/* prevent removing after the last element */
	if (!_xmmsv_list_position_normalize (&pos, l->size, 0)) {
		return 0;
	}

	xmmsv_unref (l->list[pos]);

	l->size--;

	/* fill the gap */
	if (pos < l->size) {
		memmove (l->list + pos, l->list + pos + 1,
		         (l->size - pos) * sizeof (xmmsv_t *));
	}

	/* Reduce memory usage by two if possible */
	half_size = l->allocated >> 1;
	if (l->size <= half_size) {
		int success;
		success = _xmmsv_list_resize (l, half_size);
		x_return_val_if_fail (success, 0);
	}

	/* update iterator pos */
	for (n = l->iterators; n; n = n->next) {
		it = (xmmsv_list_iter_t *) n->data;
		if (it->position > pos) {
			it->position--;
		}
	}

	return 1;
}

static int
_xmmsv_list_move (xmmsv_list_internal_t *l, int old_pos, int new_pos)
{
	xmmsv_t *v;
	xmmsv_list_iter_t *it;
	x_list_t *n;

	if (!_xmmsv_list_position_normalize (&old_pos, l->size, 0)) {
		return 0;
	}
	if (!_xmmsv_list_position_normalize (&new_pos, l->size, 0)) {
		return 0;
	}

	v = l->list[old_pos];
	if (old_pos < new_pos) {
		memmove (l->list + old_pos, l->list + old_pos + 1,
		         (new_pos - old_pos) * sizeof (xmmsv_t *));
		l->list[new_pos] = v;

		/* update iterator pos */
		for (n = l->iterators; n; n = n->next) {
			it = (xmmsv_list_iter_t *) n->data;
			if (it->position >= old_pos && it->position <= new_pos) {
				if (it->position == old_pos) {
					it->position = new_pos;
				} else {
					it->position--;
				}
			}
		}
	} else {
		memmove (l->list + new_pos + 1, l->list + new_pos,
		         (old_pos - new_pos) * sizeof (xmmsv_t *));
		l->list[new_pos] = v;

		/* update iterator pos */
		for (n = l->iterators; n; n = n->next) {
			it = (xmmsv_list_iter_t *) n->data;
			if (it->position >= new_pos && it->position <= old_pos) {
				if (it->position == old_pos) {
					it->position = new_pos;
				} else {
					it->position++;
				}
			}
		}
	}

	return 1;
}

static void
_xmmsv_list_clear (xmmsv_list_internal_t *l)
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

static void
_xmmsv_list_sort (xmmsv_list_internal_t *l, xmmsv_list_compare_func_t comparator)
{
	qsort (l->list, l->size, sizeof (xmmsv_t *),
	       (int (*)(const void *, const void *)) comparator);
}

/**
 * Allocates a new list #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_list (void)
{
	xmmsv_t *val = _xmmsv_new (XMMSV_TYPE_LIST);

	if (val) {
		val->value.list = _xmmsv_list_new ();
		val->value.list->parent_value = val;
	}

	return val;
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
	xmmsv_list_internal_t *l;

	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);

	l = listv->value.list;

	/* prevent accessing after the last element */
	if (!_xmmsv_list_position_normalize (&pos, l->size, 0)) {
		return 0;
	}

	if (val) {
		*val = l->list[pos];
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
	xmmsv_t *old_val;
	xmmsv_list_internal_t *l;

	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (val, 0);
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);

	l = listv->value.list;

	if (!_xmmsv_list_position_normalize (&pos, l->size, 0)) {
		return 0;
	}

	old_val = l->list[pos];
	l->list[pos] = xmmsv_ref (val);
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
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);
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
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);

	return _xmmsv_list_remove (listv->value.list, pos);
}

/**
 * Move the element from position #old to position #new.
 *
 * #xmmsv_list_iter_t's remain pointing at their element (which might or might
 * not be at a different position).
 *
 * @param listv A #xmmsv_t containing a list
 * @param old The original position in the list. If negative, start counting
 *            from the end (-1 is the last element, etc.)
 * @param new The new position in the list. If negative start counting from the
 *            end (-1 is the last element, etc.) For the sake of counting the
 *            element to be moved is still at its old position.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_move (xmmsv_t *listv, int old_pos, int new_pos)
{
	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);

	return _xmmsv_list_move (listv->value.list, old_pos, new_pos);
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
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);
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
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);

	_xmmsv_list_clear (listv->value.list);

	return 1;
}

/**
 * Sort the list using the supplied comparator.
 *
 * @param listv A #xmmsv_t containing a list.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_sort (xmmsv_t *listv, xmmsv_list_compare_func_t comparator)
{
	x_return_val_if_fail (comparator, 0);
	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);

	_xmmsv_list_sort (listv->value.list, comparator);

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
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);
	x_return_val_if_fail (xmmsv_get_list_iter (listv, &it), 0);

	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &v);
		func (v, user_data);
		xmmsv_list_iter_next (it);
	}

	_xmmsv_list_iter_free (it);

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
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), -1);

	return listv->value.list->size;
}


int
xmmsv_list_restrict_type (xmmsv_t *listv, xmmsv_type_t type)
{
	x_return_val_if_fail (xmmsv_list_has_type (listv, type), 0);
	x_return_val_if_fail (!listv->value.list->restricted ||
	                      listv->value.list->restricttype == type, 0);

	listv->value.list->restricted = true;
	listv->value.list->restricttype = type;

	return 1;
}

/**
 * Gets the current type restriction of a list.
 *
 * @param listv The list to Check
 * @return the xmmsv_type_t of the restricted type, or XMMSV_TYPE_NONE if no restriction.
 */
int
xmmsv_list_get_type (xmmsv_t *listv, xmmsv_type_t *type)
{
	x_return_val_if_fail (listv, false);
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), false);
	if (listv->value.list->restricted) {
		*type = listv->value.list->restricttype;
	} else {
		*type = XMMSV_TYPE_NONE;
	}
	return true;
}

/**
 * Checks if all elements in the list has the given type
 *
 * @param listv The list to check
 * @param type The type to check for
 * @return non-zero if all elements in the list has the type, 0 otherwise
 */
int
xmmsv_list_has_type (xmmsv_t *listv, xmmsv_type_t type)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *v;

	x_return_val_if_fail (listv, 0);
	x_return_val_if_fail (xmmsv_is_type (listv, XMMSV_TYPE_LIST), 0);

	if (listv->value.list->restricted)
		return listv->value.list->restricttype == type;

	x_return_val_if_fail (xmmsv_get_list_iter (listv, &it), 0);
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_list_iter_entry (it, &v);
		if (!xmmsv_is_type (v, type)) {
			_xmmsv_list_iter_free (it);
			return 0;
		}
		xmmsv_list_iter_next (it);
	}

	_xmmsv_list_iter_free (it);

	return 1;
}

static xmmsv_list_iter_t *
_xmmsv_list_iter_new (xmmsv_list_internal_t *l)
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

	new_it = _xmmsv_list_iter_new (val->value.list);
	if (!new_it) {
		*it = NULL;
		return 0;
	}

	*it = new_it;

	return 1;
}

static void
_xmmsv_list_iter_free (xmmsv_list_iter_t *it)
{
	/* unref iterator from list and free it */
	it->parent->iterators = x_list_remove (it->parent->iterators, it);
	free (it);
}

/**
 * Explicitly free list iterator.
 *
 * Immediately frees any resources used by this iterator. The iterator
 * is freed automatically when the list is freed, but this function is
 * useful when the list can be long lived.
 *
 * @param it iterator to free
 *
 */
void
xmmsv_list_iter_explicit_destroy (xmmsv_list_iter_t *it)
{
	_xmmsv_list_iter_free (it);
}

/**
 * Get the element currently pointed at by the iterator. This function
 * does not increase the refcount of the element, the reference is
 * still owned by the list. If iterator does not point on a valid
 * element xmmsv_list_iter_entry returns 0 and leaves val untouched.
 *
 * @param it A #xmmsv_list_iter_t.
 * @param val Pointer set to a borrowed reference to the element
 *            pointed at by the iterator.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_iter_entry (xmmsv_list_iter_t *it, xmmsv_t **val)
{
	if (!xmmsv_list_iter_valid (it))
		return 0;

	*val = it->parent->list[it->position];

	return 1;
}

/**
 * Check whether the iterator is valid and points to a valid element.
 *
 * @param it A #xmmsv_list_iter_t.
 * @return 1 if the iterator is valid, 0 otherwise
 */
int
xmmsv_list_iter_valid (xmmsv_list_iter_t *it)
{
	return it && (it->position < it->parent->size) && (it->position >= 0);
}

/**
 * Rewind the iterator to the start of the list.
 *
 * @param it A #xmmsv_list_iter_t.
 */
void
xmmsv_list_iter_first (xmmsv_list_iter_t *it)
{
	x_return_if_fail (it);

	it->position = 0;
}

/**
 * Move the iterator to end of the list.
 *
 * @param listv A #xmmsv_list_iter_t.
 */
void
xmmsv_list_iter_last (xmmsv_list_iter_t *it)
{
	x_return_if_fail (it);

	if (it->parent->size > 0) {
		it->position = it->parent->size - 1;
	} else {
		it->position = it->parent->size;
	}
}

/**
 * Advance the iterator to the next element in the list.
 *
 * @param it A #xmmsv_list_iter_t.
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
 * Move the iterator to the previous element in the list.
 *
 * @param listv A #xmmsv_list_iter_t.
 */
void
xmmsv_list_iter_prev (xmmsv_list_iter_t *it)
{
	x_return_if_fail (it);

	if (it->position >= 0) {
		it->position--;
	}
}


/**
 * Move the iterator to the n-th element in the list.
 *
 * @param it A #xmmsv_list_iter_t.
 * @param pos The position in the list. If negative, start counting
 *            from the end (-1 is the last element, etc).
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_iter_seek (xmmsv_list_iter_t *it, int pos)
{
	x_return_val_if_fail (it, 0);

	if (!_xmmsv_list_position_normalize (&pos, it->parent->size, 1)) {
		return 0;
	}
	it->position = pos;

	return 1;
}

/**
 * Tell the position of the iterator.
 *
 * @param it A #xmmsv_list_iter_t.
 * @return The position of the iterator, or -1 if invalid.
 */
int
xmmsv_list_iter_tell (const xmmsv_list_iter_t *it)
{
	x_return_val_if_fail (it, -1);

	return it->position;
}

/**
 * Return the parent #xmmsv_t of an iterator.
 *
 * @param it A #xmmsv_list_iter_t.
 * @return The parent #xmmsv_t of the iterator, or NULL if invalid.
 */
xmmsv_t*
xmmsv_list_iter_get_parent (const xmmsv_list_iter_t *it)
{
	x_return_val_if_fail (it, NULL);

	return it->parent->parent_value;
}

/**
 * Replace an element in the list at the position pointed at by the
 * iterator.
 *
 * @param it A #xmmsv_list_iter_t.
 * @param val The element to insert.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_iter_set (xmmsv_list_iter_t *it, xmmsv_t *val)
{
	x_return_val_if_fail (it, 0);
	x_return_val_if_fail (val, 0);

	return xmmsv_list_set (it->parent->parent_value, it->position, val);
}

/**
 * Insert an element in the list at the position pointed at by the
 * iterator.
 *
 * @param it A #xmmsv_list_iter_t.
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
 * @param it A #xmmsv_list_iter_t.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_list_iter_remove (xmmsv_list_iter_t *it)
{
	x_return_val_if_fail (it, 0);

	return _xmmsv_list_remove (it->parent, it->position);
}

static int
_xmmsv_list_flatten (xmmsv_t *list, xmmsv_t *result, int depth)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *val;
	int ret = 1;

	x_return_val_if_fail (xmmsv_is_type (list, XMMSV_TYPE_LIST), 0);

	for (xmmsv_get_list_iter (list, &it);
	     xmmsv_list_iter_entry (it, &val) && ret;
	     xmmsv_list_iter_next (it)) {
		if (depth == 0) {
			xmmsv_list_append (result, val);
		} else {
			ret = _xmmsv_list_flatten (val, result, depth - 1);
		}
	}

	return ret;
}

/**
 * Flattens a list of lists.
 *
 * @param list The list to flatten
 * @param depth The level of lists to flatten.
 * @return A new flattened list, or NULL on error.
 */
xmmsv_t *
xmmsv_list_flatten (xmmsv_t *list, int depth)
{
	x_return_val_if_fail (list, NULL);
	xmmsv_t *result = xmmsv_new_list ();

	if (!_xmmsv_list_flatten (list, result, depth)) {
		xmmsv_unref (result);
		return NULL;
	}

	return result;
}

/* macro-magically define list extractors */
#define GEN_LIST_EXTRACTOR_FUNC(typename, type) \
	int \
	xmmsv_list_get_##typename (xmmsv_t *val, int pos, type *r) \
	{ \
		xmmsv_t *v; \
		if (!xmmsv_list_get (val, pos, &v)) { \
			return 0; \
		} \
		return xmmsv_get_##typename (v, r); \
	}

GEN_LIST_EXTRACTOR_FUNC (string, const char *)
GEN_LIST_EXTRACTOR_FUNC (int32, int32_t)
GEN_LIST_EXTRACTOR_FUNC (int64, int64_t)
GEN_LIST_EXTRACTOR_FUNC (float, float)

int
xmmsv_list_get_coll (xmmsv_t *val, int pos, xmmsv_t **r)
{
	return xmmsv_list_get (val, pos, r);
}

/* macro-magically define list set functions */
#define GEN_LIST_SET_FUNC(typename, type)	  \
	int \
	xmmsv_list_set_##typename (xmmsv_t *list, int pos, type elem) \
	{ \
		int ret; \
		xmmsv_t *v; \
	  \
		v = xmmsv_new_##typename (elem); \
			ret = xmmsv_list_set (list, pos, v); \
			xmmsv_unref (v); \
	  \
			return ret; \
	}

GEN_LIST_SET_FUNC (string, const char *)
GEN_LIST_SET_FUNC (int, int64_t)
GEN_LIST_SET_FUNC (float, float)

int
xmmsv_list_set_coll (xmmsv_t *list, int pos, xmmsv_t *elem)
{
	return xmmsv_list_set (list, pos, elem);
}

/* macro-magically define list insert functions */
#define GEN_LIST_INSERT_FUNC(typename, type)	  \
	int \
	xmmsv_list_insert_##typename (xmmsv_t *list, int pos, type elem) \
	{ \
		int ret; \
		xmmsv_t *v; \
	  \
		v = xmmsv_new_##typename (elem); \
			ret = xmmsv_list_insert (list, pos, v); \
			xmmsv_unref (v); \
	  \
			return ret; \
	}

GEN_LIST_INSERT_FUNC (string, const char *)
GEN_LIST_INSERT_FUNC (int, int64_t)
GEN_LIST_INSERT_FUNC (float, float)

int
xmmsv_list_insert_coll (xmmsv_t *list, int pos, xmmsv_t *elem)
{
	return xmmsv_list_insert (list, pos, elem);
}

/* macro-magically define list append functions */
#define GEN_LIST_APPEND_FUNC(typename, type)	  \
	int \
	xmmsv_list_append_##typename (xmmsv_t *list, type elem) \
	{ \
		int ret; \
		xmmsv_t *v; \
	  \
		v = xmmsv_new_##typename (elem); \
			ret = xmmsv_list_append (list, v); \
			xmmsv_unref (v); \
	  \
			return ret; \
	}

GEN_LIST_APPEND_FUNC (string, const char *)
GEN_LIST_APPEND_FUNC (int, int64_t)
GEN_LIST_APPEND_FUNC (float, float)

int
xmmsv_list_append_coll (xmmsv_t *list, xmmsv_t *elem)
{
	return xmmsv_list_append (list, elem);
}

/* macro-magically define list_iter extractors */
#define GEN_LIST_ITER_EXTRACTOR_FUNC(typename, type)	  \
	int \
	xmmsv_list_iter_entry_##typename (xmmsv_list_iter_t *it, type *r) \
	{ \
		xmmsv_t *v; \
		if (!xmmsv_list_iter_entry (it, &v)) { \
			return 0; \
		} \
		return xmmsv_get_##typename (v, r); \
	}

GEN_LIST_ITER_EXTRACTOR_FUNC (string, const char *)
GEN_LIST_ITER_EXTRACTOR_FUNC (int32, int32_t)
GEN_LIST_ITER_EXTRACTOR_FUNC (int64, int64_t)
GEN_LIST_ITER_EXTRACTOR_FUNC (float, float)

int
xmmsv_list_iter_entry_coll (xmmsv_list_iter_t *it, xmmsv_t **r)
{
	return xmmsv_list_iter_entry (it, r);
}

/* macro-magically define list_iter insert functions */
#define GEN_LIST_ITER_INSERT_FUNC(typename, type)	  \
	int \
	xmmsv_list_iter_insert_##typename (xmmsv_list_iter_t *it, type elem) \
	{ \
		int ret; \
		xmmsv_t *v; \
	  \
		v = xmmsv_new_##typename (elem); \
			ret = xmmsv_list_iter_insert (it, v); \
			xmmsv_unref (v); \
	  \
			return ret; \
	}

GEN_LIST_ITER_INSERT_FUNC (string, const char *)
GEN_LIST_ITER_INSERT_FUNC (int, int64_t)
GEN_LIST_ITER_INSERT_FUNC (float, float)

int
xmmsv_list_iter_insert_coll (xmmsv_list_iter_t *it, xmmsv_t *elem)
{
	return xmmsv_list_iter_insert (it, elem);
}
