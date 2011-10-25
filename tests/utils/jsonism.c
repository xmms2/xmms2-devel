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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xmmsc/xmmsv.h"
#include "jsonism.h"
#include "utils/json.h"

static xmmsv_t *
create_structure (int stack_offset, int is_object)
{
	if (is_object) {
		return xmmsv_new_dict ();
	} else {
		return xmmsv_new_list ();
	}
}

static xmmsv_t *
create_data (int type, const char *data, uint32_t len)
{
	switch (type) {
		case JSON_STRING:
			return xmmsv_new_string (data);
		case JSON_INT:
			return xmmsv_new_int (atoi(data));
		case JSON_FLOAT:
			return xmmsv_new_error ("Float type not supported");
		case JSON_NULL:
			return xmmsv_new_none ();
		case JSON_TRUE:
			return xmmsv_new_int (1);
		case JSON_FALSE:
			return xmmsv_new_int (0);
		default:
			return xmmsv_new_error ("Unknown data type.");
	}
}

static int
append (xmmsv_t *obj, const char *key, uint32_t key_len, xmmsv_t *value)
{
	if (xmmsv_is_type (obj, XMMSV_TYPE_LIST)) {
		xmmsv_list_append (obj, value);
	} else if (xmmsv_is_type (obj, XMMSV_TYPE_DICT) && key) {
		xmmsv_dict_set (obj, key, value);
	} else {
		/* Should never be reached */
		assert (0);
	}
	xmmsv_unref (value);
	return 0;
}

xmmsv_t *
xmmsv_from_json (const char *spec)
{
	json_config conf = {
		0, /* buffer_initial_size (0=default) */
		0, /* max_nesting (0=no limit) */
		0, /* max_data (0=no limit) */
		1, /* allow_c_comments */
		0, /* allow_yaml_comments */
		NULL, /* user_calloc */
		NULL /* user_realloc */
	};
	json_parser_dom dom;
	json_parser parser;
	xmmsv_t *value;
	int error;

	json_parser_dom_init (&dom,
						  (json_parser_dom_create_structure) create_structure,
						  (json_parser_dom_create_data) create_data,
						  (json_parser_dom_append) append);
	json_parser_init (&parser, &conf, json_parser_dom_callback, &dom);

	error = json_parser_string (&parser, spec, strlen (spec), NULL);
	if (error != 0) {
		switch (error) {
			case JSON_ERROR_BAD_CHAR:
				fprintf (stderr, "Failed to parse due to bad character!\n");
				break;
			case JSON_ERROR_UNEXPECTED_CHAR:
				fprintf (stderr, "Failed to parse due to unexpected character!\n");
				break;
			case JSON_ERROR_NO_MEMORY:
				fprintf (stderr, "Failed to parse (%d)!\n", error);
				break;
		}
	}

	assert (dom.root_structure != NULL);
	assert (dom.stack_offset == 0);

	value = (xmmsv_t *) dom.root_structure;

	json_parser_dom_free (&dom);
	json_parser_free (&parser);

	return value;
}
