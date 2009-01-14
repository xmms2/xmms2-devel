/* GLIB - Library of useful routines for C programming
 *  Copyright (C) 2003-2009 XMMS2 Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <assert.h>

#include "xmmspriv/xmms_list.h"
#include "xmmsclientpriv/xmmsclient_util.h"

#include <stdlib.h>

#define _x_list_alloc x_list_alloc
x_list_t *
x_list_alloc (void)
{
	x_list_t *list;

	list = calloc (1, sizeof (x_list_t));

	return list;
}

void
x_list_free (x_list_t *list)
{
	x_list_t *last;

	while (list) {
		last = list;
		list = list->next;
		free (last);
	}
}

#define _x_list_free_1 x_list_free_1
void
x_list_free_1 (x_list_t *list)
{
	 free (list);
}

x_list_t*
x_list_append (x_list_t *list, void *data)
{
	x_list_t *new_list;
	x_list_t *last;

	new_list = _x_list_alloc ();
	new_list->data = data;

	if (list) {
		last = x_list_last (list);
		/* g_assert (last != NULL); */
		last->next = new_list;
		new_list->prev = last;

		return list;
	} else {
		return new_list;
	}
}

x_list_t*
x_list_prepend (x_list_t *list, void *data)
{
	x_list_t *new_list;

	new_list = _x_list_alloc ();
	new_list->data = data;

	if (list) {
		if (list->prev) {
			list->prev->next = new_list;
			new_list->prev = list->prev;
		}
		list->prev = new_list;
		new_list->next = list;
	}

	return new_list;
}

x_list_t*
x_list_insert (x_list_t *list, void *data, int position)
{
	x_list_t *new_list;
	x_list_t *tmp_list;

	if (position < 0) {
		return x_list_append (list, data);
	} else if (position == 0) {
		return x_list_prepend (list, data);
	}

	tmp_list = x_list_nth (list, position);
	if (!tmp_list)
		return x_list_append (list, data);

	new_list = _x_list_alloc ();
	new_list->data = data;

	if (tmp_list->prev) {
		tmp_list->prev->next = new_list;
		new_list->prev = tmp_list->prev;
	}
	new_list->next = tmp_list;
	tmp_list->prev = new_list;

	if (tmp_list == list) {
		return new_list;
	} else {
		return list;
	}
}

x_list_t*
x_list_insert_before (x_list_t *list, x_list_t *sibling, void *data)
{
	if (!list) {
		list = x_list_alloc ();
		list->data = data;
		assert (sibling);
		return list;
	} else if (sibling) {
		x_list_t *node;

		node = x_list_alloc ();
		node->data = data;
		if (sibling->prev) {
			node->prev = sibling->prev;
			node->prev->next = node;
			node->next = sibling;
			sibling->prev = node;
			return list;
		} else {
			node->next = sibling;
			sibling->prev = node;
			assert (sibling);
			return node;
		}
	} else {
		x_list_t *last;

		last = list;
		while (last->next) {
			last = last->next;
		}

		last->next = x_list_alloc ();
		last->next->data = data;
		last->next->prev = last;

		return list;
	}
}

x_list_t *
x_list_concat (x_list_t *list1, x_list_t *list2)
{
	x_list_t *tmp_list;

	if (list2) {
		tmp_list = x_list_last (list1);
		if (tmp_list) {
			tmp_list->next = list2;
		} else {
			list1 = list2;
		}
		list2->prev = tmp_list;
	}

	return list1;
}

x_list_t*
x_list_remove (x_list_t *list, const void *data)
{
	x_list_t *tmp;

	tmp = list;
	while (tmp) {
		if (tmp->data != data) {
			tmp = tmp->next;
		} else {
			if (tmp->prev)
				tmp->prev->next = tmp->next;
			if (tmp->next)
				tmp->next->prev = tmp->prev;

			if (list == tmp)
				list = list->next;

			_x_list_free_1 (tmp);

			break;
		}
	}
	return list;
}

x_list_t*
x_list_remove_all (x_list_t *list, const void * data)
{
	x_list_t *tmp = list;

	while (tmp) {
		if (tmp->data != data) {
			tmp = tmp->next;
		} else {
			x_list_t *next = tmp->next;

			if (tmp->prev) {
				tmp->prev->next = next;
			} else {
				list = next;
			}
			if (next) {
				next->prev = tmp->prev;
			}

			_x_list_free_1 (tmp);
			tmp = next;
		}
	}
	return list;
}

static inline x_list_t*
_x_list_remove_link (x_list_t *list, x_list_t *link)
{
	if (link) {
		if (link->prev)
			link->prev->next = link->next;
		if (link->next)
			link->next->prev = link->prev;

		if (link == list)
			list = list->next;

		link->next = NULL;
		link->prev = NULL;
	}

	return list;
}

x_list_t*
x_list_remove_link (x_list_t *list, x_list_t *link)
{
	return _x_list_remove_link (list, link);
}

x_list_t*
x_list_delete_link (x_list_t *list, x_list_t *link)
{
	list = _x_list_remove_link (list, link);
	_x_list_free_1 (link);

	return list;
}

x_list_t*
x_list_copy (x_list_t *list)
{
	x_list_t *new_list = NULL;

	if (list) {
		x_list_t *last;

		new_list = _x_list_alloc ();
		new_list->data = list->data;
		last = new_list;
		list = list->next;
		while (list) {
			last->next = _x_list_alloc ();
			last->next->prev = last;
			last = last->next;
			last->data = list->data;
			list = list->next;
		}
	}

	return new_list;
}

x_list_t*
x_list_reverse (x_list_t *list)
{
	x_list_t *last;

	last = NULL;
	while (list) {
		last = list;
		list = last->next;
		last->next = last->prev;
		last->prev = list;
	}

	return last;
}

x_list_t*
x_list_nth (x_list_t *list, unsigned int n)
{
	while ((n-- > 0) && list)
		list = list->next;

	return list;
}

x_list_t*
x_list_nth_prev (x_list_t *list, unsigned int n)
{
	while ((n-- > 0) && list)
		list = list->prev;

	return list;
}

void *
x_list_nth_data (x_list_t *list, unsigned int n)
{
	while ((n-- > 0) && list)
		list = list->next;

	return list ? list->data : NULL;
}

x_list_t*
x_list_find (x_list_t *list, const void *data)
{
	while (list) {
		if (list->data == data)
			break;
		list = list->next;
	}

	return list;
}

x_list_t*
x_list_find_custom (x_list_t *list, const void *data, XCompareFunc func)
{
	assert (func != NULL);

	while (list) {
		if (! func (list->data, data))
			return list;
		list = list->next;
	}

	return NULL;
}


int
x_list_position (x_list_t *list, x_list_t *link)
{
	int i;

	i = 0;
	while (list) {
		if (list == link)
			return i;
		i++;
		list = list->next;
	}

	return -1;
}

int
x_list_index (x_list_t *list, const void *data)
{
	int i;

	i = 0;
	while (list) {
		if (list->data == data)
			return i;
		i++;
		list = list->next;
	}

	return -1;
}

x_list_t*
x_list_last (x_list_t *list)
{
	if (list) {
		while (list->next)
			list = list->next;
	}

	return list;
}

x_list_t*
x_list_first (x_list_t *list)
{
	if (list) {
		while (list->prev)
			list = list->prev;
	}

	return list;
}

unsigned int
x_list_length (x_list_t *list)
{
	unsigned int length;

	length = 0;
	while (list) {
		length++;
		list = list->next;
	}

	return length;
}

void
x_list_foreach (x_list_t *list, XFunc func, void *user_data)
{
	while (list) {
		x_list_t *next = list->next;
		(*func) (list->data, user_data);
		list = next;
	}
}


x_list_t*
x_list_insert_sorted (x_list_t *list, void *data, XCompareFunc func)
{
	x_list_t *tmp_list = list;
	x_list_t *new_list;
	int cmp;

	assert (func != NULL);

	if (!list) {
		new_list = _x_list_alloc ();
		new_list->data = data;
		return new_list;
	}

	cmp = (*func) (data, tmp_list->data);

	while ((tmp_list->next) && (cmp > 0)) {
		tmp_list = tmp_list->next;
		cmp = (*func) (data, tmp_list->data);
	}

	new_list = _x_list_alloc ();
	new_list->data = data;

	if ((!tmp_list->next) && (cmp > 0)) {
		tmp_list->next = new_list;
		new_list->prev = tmp_list;
		return list;
	}

	if (tmp_list->prev) {
		tmp_list->prev->next = new_list;
		new_list->prev = tmp_list->prev;
	}
	new_list->next = tmp_list;
	tmp_list->prev = new_list;

	if (tmp_list == list)
		return new_list;
	else
		return list;
}
