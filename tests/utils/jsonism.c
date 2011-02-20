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

/* psuedo grammar
 * START :: ENTRY
 * DICT :: '{' DICT_ENTRY? (',' DICT_ENTRY)* '}'
 * DICT_ENTRY :: STRING ':' ENTRY
 * LIST :: '[' ENTRY? (',' ENTRY)* ']'
 * ENTRY :: STRING | NUMBER | LIST | DICT
 * STRING :: '\'' [a-zA-Z0-9]* '\''
 * NUMBER :: [0-9]+
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "xmmsc/xmmsv.h"
#include "jsonism.h"

static xmmsv_t *parse_string (char **ptr);
static xmmsv_t *parse_dict (char **ptr);
static xmmsv_t *parse_list (char **ptr);
static xmmsv_t *parse_number (char **ptr);

static void
eat (char **p, char c)
{
	while (*p && **p == c) (*p)++;
	assert (*p != NULL);
}

static void
munch (char **p, char c)
{
	while (*p && (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t')) (*p)++;
	assert (*p != NULL);
	while (*p && **p == c) (*p)++;
	assert (*p != NULL);
	while (*p && (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t')) (*p)++;
	assert (*p != NULL);
}

static xmmsv_t *
parse_number (char **ptr)
{
	char *end;
	assert (**ptr >= '0' && **ptr <= '9');
	for (end = *ptr; *end >= '0' && *end <= '9'; end++);
	char *number = strndup (*ptr, end - *ptr);
	char *endptr = NULL;
	int ival = strtol (number, &endptr, 10);
	assert (endptr != number && *endptr == '\0');
	xmmsv_t *value = xmmsv_new_int (ival);
	free (number);
	*ptr = end;
	return value;
}

static xmmsv_t *
parse_entry (char **ptr)
{
	if (**ptr == '\'') {
		return parse_string (ptr);
	} else if (**ptr == '{') {
		return parse_dict (ptr);
	} else if (**ptr == '[') {
		return parse_list (ptr);
	} else {
		return parse_number (ptr);
	}
}

static xmmsv_t *
parse_list (char **ptr)
{
	munch (ptr, '[');
	xmmsv_t *list = xmmsv_new_list ();
	while (**ptr != ']') {
		xmmsv_t *entry = parse_entry (ptr);
		xmmsv_list_append (list, entry);
		xmmsv_unref (entry);
		munch (ptr, ',');
	};
	munch (ptr, ']');
	return list;
}

static char *
parse_cstring (char **ptr)
{
	char *end, *value;
	eat (ptr, '\'');
	end = strchr (*ptr, '\'');
	assert (end != NULL);
	value = strndup (*ptr, end - *ptr);
	*ptr = end;
	eat (ptr, '\'');
	return value;
}

static xmmsv_t *
parse_string (char **ptr)
{
	char *str = parse_cstring (ptr);
	xmmsv_t *value = xmmsv_new_string (str);
	free (str);
	return value;
}


static void
parse_dict_entry (char **ptr, xmmsv_t *dict)
{
	xmmsv_t *value;
	char *key;
	key = parse_cstring (ptr);
	munch (ptr, ':');
	value = parse_entry (ptr);
	xmmsv_dict_set (dict, key, value);
	xmmsv_unref (value);
	free (key);
}

static xmmsv_t *
parse_dict (char **ptr)
{
	munch (ptr, '{');
	xmmsv_t *dict = xmmsv_new_dict ();
	while (**ptr != '}') {
		parse_dict_entry (ptr, dict);
		munch (ptr, ',');
	};
	munch (ptr, '}');
	return dict;
}

xmmsv_t *
xmmsv_from_json (const char *spec)
{
	char *data, *ptr;
	ptr = data = strdup (spec);
	eat (&ptr, ' ');
	xmmsv_t *value = parse_entry (&ptr);
	free (data);
	return value;
}
