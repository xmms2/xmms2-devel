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
#include <string.h>
#include <assert.h>

#include "xmmsc/xmmsv.h"
#include "utils/jsonism.h"
#include "utils/coll_utils.h"
#include "utils/value_utils.h"

static xmmsv_coll_t *parse_collection (xmmsv_t *attrs);

static const char *coll_types[XMMS_COLLECTION_TYPE_LAST + 1] = {
	"reference",
	"universe",
	"union",
	"intersection",
	"complement",
	"has",
	"match",
	"token",
	"equals",
	"notequal",
	"smaller",
	"smallereq",
	"greater",
	"greatereq",
	"order",
	"limit",
	"mediaset",
	"idlist"
};

/**
 * Parse a Collection from a dict.
 * Each dict contains:
 *  - type, the name of the Collection
 *  - operands, a list of Collections [optional]
 *  - attributes, a dict of attributes [optional]
 *  - idlist, a list of media library id's [optional]
 */
xmmsv_coll_t *
xmmsv_coll_from_string (const char *data)
{
	xmmsv_coll_t *collection;
	xmmsv_t *dict;

	dict = xmmsv_from_json (data);
	collection = parse_collection (dict);
	xmmsv_unref (dict);

	return collection;
}

/**
 * Build a collection from an already parsed dictionary
 */
xmmsv_coll_t *
xmmsv_coll_from_dict (xmmsv_t *data)
{
	return parse_collection (data);
}

static const char *
collection_type_string (xmmsv_coll_type_t type)
{
	if (type < 0 || type > XMMS_COLLECTION_TYPE_LAST) {
		return "unknown";
	}
	return coll_types[type];
}

static int
collection_type_from_string (const char *name, xmmsv_coll_type_t *type)
{
	if (name == NULL) {
		return 0;
	} else if (strcmp ("reference", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_REFERENCE;
	} else if (strcmp ("universe", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_UNIVERSE;
	} else if (strcmp ("union", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_UNION;
	} else if (strcmp ("intersection", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_INTERSECTION;
	} else if (strcmp ("complement", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_COMPLEMENT;
	} else if (strcmp ("has", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_HAS;
	} else if (strcmp ("match", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_MATCH;
	} else if (strcmp ("token", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_TOKEN;
	} else if (strcmp ("equals", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_EQUALS;
	} else if (strcmp ("notequal", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_NOTEQUAL;
	} else if (strcmp ("smaller", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_SMALLER;
	} else if (strcmp ("smallereq", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_SMALLEREQ;
	} else if (strcmp ("greater", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_GREATER;
	} else if (strcmp ("greatereq", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_GREATEREQ;
	} else if (strcmp ("order", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_ORDER;
	} else if (strcmp ("limit", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_LIMIT;
	} else if (strcmp ("mediaset", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_MEDIASET;
	} else if (strcmp ("idlist", name) == 0) {
		*type = XMMS_COLLECTION_TYPE_IDLIST;
	} else {
		return 1;
	}

	return 1;
}


static void
parse_idlist (xmmsv_coll_t *coll, xmmsv_t *list)
{
	xmmsv_list_iter_t *it;

	assert (xmmsv_is_type (list, XMMSV_TYPE_LIST));
	assert (xmmsv_get_list_iter (list, &it));

	while (xmmsv_list_iter_valid (it)) {
		int32_t id;
		assert (xmmsv_list_iter_entry_int (it, &id));
		assert (xmmsv_coll_idlist_append (coll, id));
		xmmsv_list_iter_next (it);
	}
}

static void
parse_attributes (xmmsv_coll_t *coll, xmmsv_t *attrs)
{
	xmmsv_dict_iter_t *it;

	assert (xmmsv_is_type (attrs, XMMSV_TYPE_DICT));
	assert (xmmsv_get_dict_iter (attrs, &it));

	while (xmmsv_dict_iter_valid (it)) {
		xmmsv_t *entry;
		const char *key, *value;

		assert (xmmsv_dict_iter_pair (it, &key, &entry));
		assert (xmmsv_get_string (entry, &value));

		xmmsv_coll_attribute_set (coll, key, value);

		xmmsv_dict_iter_next (it);
	}
}

static void
parse_operands (xmmsv_coll_t *coll, xmmsv_t *operands)
{
	xmmsv_list_iter_t *it;

	assert (xmmsv_is_type (operands, XMMSV_TYPE_LIST));
	assert (xmmsv_get_list_iter (operands, &it));

	while (xmmsv_list_iter_valid (it)) {
		xmmsv_coll_t *operand;
		xmmsv_t *entry;

		assert (xmmsv_list_iter_entry (it, &entry));
		operand = parse_collection (entry);
		assert (operand != NULL);

		xmmsv_coll_add_operand (coll, operand);
		xmmsv_coll_unref (operand);

		xmmsv_list_iter_next (it);
	}
}

static xmmsv_coll_t *
parse_collection (xmmsv_t *dict)
{
	xmmsv_coll_type_t type = 0;
	xmmsv_coll_t *coll;
	xmmsv_t *attributes, *operands, *list;
	const char *name;

	assert (xmmsv_is_type (dict, XMMSV_TYPE_DICT));
	xmmsv_dict_entry_get_string (dict, "type", &name);
	assert (collection_type_from_string (name, &type));

	coll = xmmsv_coll_new (type);

	if (xmmsv_dict_get (dict, "attributes", &attributes))
		parse_attributes (coll, attributes);

	if (xmmsv_dict_get (dict, "operands", &operands))
		parse_operands (coll, operands);

	if (xmmsv_dict_get (dict, "idlist", &list))
		parse_idlist (coll, list);

	return coll;
}

int
xmmsv_coll_compare (xmmsv_coll_t *a, xmmsv_coll_t *b)
{
	xmmsv_coll_type_t type;
	xmmsv_t *_a, *_b;

	type = xmmsv_coll_get_type (a);
	if (xmmsv_coll_get_type (b) != type) {
		return 0;
	}

	_a = xmmsv_coll_idlist_get (a);
	_b = xmmsv_coll_idlist_get (b);
	if (!xmmsv_compare (_a, _b)) {
		return 0;
	}

	_a = xmmsv_coll_attributes_get (a);
	_b = xmmsv_coll_attributes_get (b);
	if (!xmmsv_compare (_a, _b)) {
		return 0;
	}

	_a = xmmsv_coll_operands_get (a);
	_b = xmmsv_coll_operands_get (b);
	if (!xmmsv_compare (_a, _b)) {
		return 0;
	}

	return 1;
}

static void
_xmmsv_coll_dump_indent (int indent)
{
	int i;
	for (i = 0; i < indent; i++)
		printf ("  ");
}

static void
_xmms_coll_dump (xmmsv_coll_t *coll, int indent)
{
	xmmsv_t *attributes, *operands, *idlist;
	const char *type_str;

	printf ("/* collection */ {\n");

	type_str = collection_type_string (xmmsv_coll_get_type (coll));
	_xmmsv_coll_dump_indent (indent + 1);
	printf ("'type': '%s'", type_str);

	attributes = xmmsv_coll_attributes_get (coll);
	if (xmmsv_dict_get_size (attributes)) {
		printf (",\n");
		_xmmsv_coll_dump_indent (indent + 1);
		printf ("'attributes': ");
		xmmsv_dump_indented (attributes, indent + 1);
	}
	operands = xmmsv_coll_operands_get (coll);
	if (xmmsv_list_get_size (operands)) {
		printf (",\n");
		_xmmsv_coll_dump_indent (indent + 1);
		printf ("'operands': ");
		xmmsv_dump_indented (operands, indent + 1);
	}
	idlist = xmmsv_coll_idlist_get (coll);
	if (xmmsv_list_get_size (idlist)) {
		printf (",\n");
		_xmmsv_coll_dump_indent (indent + 1);
		printf ("'idlist': ");
		xmmsv_dump_indented (idlist, indent + 1);
	}

	printf ("\n");
	_xmmsv_coll_dump_indent (indent);
	printf ("}");
}

void
xmmsv_coll_dump_indented (xmmsv_coll_t *coll, int indent)
{
	_xmms_coll_dump (coll, indent);
}

void
xmmsv_coll_dump (xmmsv_coll_t *coll)
{
	_xmms_coll_dump (coll, 0);
	printf ("\n");
}
