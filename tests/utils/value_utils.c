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

#include <stdlib.h> /* free() */
#include <stdio.h>
#include <string.h>

#include "value_utils.h"
#include "coll_utils.h"

static int _xmmsv_compare (xmmsv_t *a, xmmsv_t *b, int ordered);

int
xmmsv_compare (xmmsv_t *a, xmmsv_t *b)
{
	return _xmmsv_compare (a, b, 1);
}

int
xmmsv_compare_unordered (xmmsv_t *a, xmmsv_t *b)
{
	return _xmmsv_compare (a, b, 0);
}

static int
_xmmsv_compare (xmmsv_t *a, xmmsv_t *b, int ordered)
{
	int type;

	type = xmmsv_get_type (a);
	if (type != xmmsv_get_type (b))
		return 0;

	switch (type) {
	case XMMSV_TYPE_STRING: {
		const char *sa, *sb;

		if (!xmmsv_get_string (a, &sa) || !xmmsv_get_string (b, &sb))
			return 0;

		if (strcmp (sa, sb) != 0)
			return 0;

		break;
	}
	case XMMSV_TYPE_INT32: {
		int ia, ib;

		if (!xmmsv_get_int (a, &ia) || !xmmsv_get_int (b, &ib))
			return 0;

		if (ia != ib)
			return 0;

		break;
	}
	case XMMSV_TYPE_DICT: {
		xmmsv_dict_iter_t *it;

		if (xmmsv_dict_get_size (a) != xmmsv_dict_get_size (b))
			return 0;

		xmmsv_get_dict_iter (a, &it);
		while (xmmsv_dict_iter_valid (it)) {
			xmmsv_t *ea, *eb;
			const char *key;

			xmmsv_dict_iter_pair (it, &key, &ea);

			if (!xmmsv_dict_get (b, key, &eb))
				return 0;

			if (!_xmmsv_compare (ea, eb, ordered))
				return 0;

			xmmsv_dict_iter_next (it);
		}
		xmmsv_dict_iter_explicit_destroy (it);

		break;
	}
	case XMMSV_TYPE_LIST: {
		int size, i, j;
		int match = 1;
		char *matched = NULL;

		size = xmmsv_list_get_size (a);
		if (size != xmmsv_list_get_size (b))
			return 0;

		if (size && !ordered) {
			matched = (char *) calloc (size, sizeof (char));
		}

		for (i = 0; i < size; i++) {
			xmmsv_t *ea, *eb;

			if (!xmmsv_list_get (a, i, &ea) || !xmmsv_list_get (b, i, &eb)) {
				match = 0;
				break;
			}

			if (ordered) {
				match = _xmmsv_compare (ea, eb, ordered);
				if (!match)
					break;
			} else {
				match = 0;
				for (j = 0; j < size; j++) {
					if (matched[j])
						continue;
					if (!xmmsv_list_get (b, j, &eb))
						break;
					if (_xmmsv_compare (ea, eb, ordered)) {
						matched[j] = 1;
						match = 1;
						break;
					}
				}
				if (!match)
					return 0;
			}
		}

		if (matched)
			free (matched);

		return match;
	}
	case XMMSV_TYPE_COLL: {
		return xmmsv_coll_compare (a, b);
	}
	default: {
		return 0;
	}
	}

	return 1;
}

static void
_xmms_dump_indent (int indent)
{
	int i;
	for (i = 0; i < indent; i++)
		printf ("  ");
}

static void
_xmms_dump (xmmsv_t *value, int indent)
{
	int type;

	if (value == NULL) {
		printf ("xmmsv_t is NULL!\n");
		return;
	}

	if (xmmsv_is_error (value)) {
		const char *message;
		xmmsv_get_error (value, &message);
		printf ("error: %s\n", message);
		return;
	}

	type = xmmsv_get_type (value);

	switch (type) {
	case XMMSV_TYPE_INT32: {
		int val;
		xmmsv_get_int (value, &val);
		printf ("%d", val);
		break;
	}
	case XMMSV_TYPE_STRING: {
		const char *val;
		xmmsv_get_string (value, &val);
		printf ("'%s'", val);
		break;
	}
	case XMMSV_TYPE_LIST: {
		xmmsv_list_iter_t *iter;
		xmmsv_get_list_iter (value, &iter);

		printf ("[");
		while (xmmsv_list_iter_valid (iter)) {
			xmmsv_t *item;

			xmmsv_list_iter_entry (iter, &item);

			_xmms_dump (item, indent + 1);

			xmmsv_list_iter_next (iter);
			if (xmmsv_list_iter_valid (iter))
				printf (", ");
		}
		printf ("]");
		break;
	}
	case XMMSV_TYPE_DICT: {
		xmmsv_dict_iter_t *iter;

		xmmsv_get_dict_iter (value, &iter);

		printf ("{\n");
		while (xmmsv_dict_iter_valid (iter)) {
			const char *key;
			xmmsv_t *item;

			xmmsv_dict_iter_pair (iter, &key, &item);

			_xmms_dump_indent (indent + 1);
			printf ("'%s': ", key);
			_xmms_dump (item, indent + 1);

			xmmsv_dict_iter_next (iter);
			if (xmmsv_dict_iter_valid (iter))
				printf (", ");
			printf ("\n");
		}
		_xmms_dump_indent (indent);
		printf ("}");

		break;
	}
	case XMMSV_TYPE_COLL: {
		xmmsv_coll_dump_indented (value, indent);
		break;
	}
	default:
		printf ("invalid type: %d\n", type);
	}
}

void
xmmsv_dump_indented (xmmsv_t *value, int indent)
{
	_xmms_dump (value, indent);
}

void
xmmsv_dump (xmmsv_t *value)
{
	_xmms_dump (value, 0);
	printf ("\n");
}
