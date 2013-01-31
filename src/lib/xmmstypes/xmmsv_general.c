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

#include <stdlib.h>
#include <string.h>

#include "xmmscpriv/xmmsv.h"
#include "xmmscpriv/xmmsc_util.h"

#include "xmmsc/xmmsv.h"
#include "xmmsc/xmmsc_util.h"

/** @file */

/* Default source preferences for accessing "propdicts" */
const char *xmmsv_default_source_pref[] = {
	"server",
	"client/*",
	"plugin/playlist",
	"plugin/id3v2",
	"plugin/segment",
	"plugin/*",
	"*",
	NULL
};


/**
 * Allocates new #xmmsv_t and references it.
 * @internal
 */
xmmsv_t *
_xmmsv_new (xmmsv_type_t type)
{
	xmmsv_t *val;

	val = x_new0 (xmmsv_t, 1);
	if (!val) {
		x_oom ();
		return NULL;
	}

	val->type = type;

	return xmmsv_ref (val);
}

/**
 * Free a #xmmsv_t along with its internal data.
 * @internal
 */
static void
_xmmsv_free (xmmsv_t *val)
{
	x_return_if_fail (val);

	switch (val->type) {
		case XMMSV_TYPE_NONE :
		case XMMSV_TYPE_END :
		case XMMSV_TYPE_INT32 :
			break;
		case XMMSV_TYPE_ERROR :
			free (val->value.error);
			val->value.error = NULL;
			break;
		case XMMSV_TYPE_STRING :
			free (val->value.string);
			val->value.string = NULL;
			break;
		case XMMSV_TYPE_COLL:
			_xmmsv_coll_free (val->value.coll);
			val->value.coll = NULL;
			break;
		case XMMSV_TYPE_BIN :
			free (val->value.bin.data);
			val->value.bin.len = 0;
			break;
		case XMMSV_TYPE_LIST:
			_xmmsv_list_free (val->value.list);
			val->value.list = NULL;
			break;
		case XMMSV_TYPE_DICT:
			_xmmsv_dict_free (val->value.dict);
			val->value.dict = NULL;
			break;
		case XMMSV_TYPE_BITBUFFER:
			if (!val->value.bit.ro && val->value.bit.buf) {
				free (val->value.bit.buf);
			}
			val->value.bit.buf = NULL;
			break;
	}

	free (val);
}


/**
 * Allocates a new empty #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_none (void)
{
	xmmsv_t *val = _xmmsv_new (XMMSV_TYPE_NONE);
	return val;
}

/**
 * Allocates a new error #xmmsv_t.
 * @param s The error message to store in the #xmmsv_t. The
 * string is copied in the value.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_error (const char *errstr)
{
	xmmsv_t *val = _xmmsv_new (XMMSV_TYPE_ERROR);

	if (val) {
		val->value.error = strdup (errstr);
	}

	return val;
}

/**
 * Allocates a new integer #xmmsv_t.
 * @param i The value to store in the #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_int (int32_t i)
{
	xmmsv_t *val = _xmmsv_new (XMMSV_TYPE_INT32);

	if (val) {
		val->value.int32 = i;
	}

	return val;
}

/**
 * Allocates a new string #xmmsv_t.
 * @param s The value to store in the #xmmsv_t. The string is
 * copied in the value.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_string (const char *s)
{
	xmmsv_t *val;

	x_return_val_if_fail (s, NULL);
	x_return_val_if_fail (xmmsv_utf8_validate (s), NULL);

	val = _xmmsv_new (XMMSV_TYPE_STRING);
	if (val) {
		val->value.string = strdup (s);
	}

	return val;
}

/**
 * Allocates a new binary data #xmmsv_t.
 * @param data The data to store in the #xmmsv_t.
 * @param len The size of the data.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_bin (const unsigned char *data, unsigned int len)
{
	xmmsv_t *val = _xmmsv_new (XMMSV_TYPE_BIN);

	if (val) {
		/* copy the data! */
		val->value.bin.data = x_malloc (len);
		if (!val->value.bin.data) {
			free (val);
			x_oom ();
			return NULL;
		}
		memcpy (val->value.bin.data, data, len);
		val->value.bin.len = len;
	}

	return val;
}

/**
 * References the #xmmsv_t
 *
 * @param val the value to reference.
 * @return val
 */
xmmsv_t *
xmmsv_ref (xmmsv_t *val)
{
	x_return_val_if_fail (val, NULL);
	val->ref++;

	return val;
}

/**
 * Decreases the references for the #xmmsv_t
 * When the number of references reaches 0 it will
 * be freed. And thus all data you extracted from it
 * will be deallocated.
 */
void
xmmsv_unref (xmmsv_t *val)
{
	x_return_if_fail (val);
	x_api_error_if (val->ref < 1, "with a freed value",);

	val->ref--;
	if (val->ref == 0) {
		_xmmsv_free (val);
	}
}

/**
 * Get the type of the value.
 *
 * @param val a #xmmsv_t to get the type from.
 * @returns The data type in the value.
 */
xmmsv_type_t
xmmsv_get_type (const xmmsv_t *val)
{
	x_api_error_if (!val, "NULL value",
	                XMMSV_TYPE_NONE);

	return val->type;
}

/**
 * Check if value is of specified type.
 *
 * @param val #xmmsv_t to check.
 * @param t #xmmsv_type_t to check for.
 * @return 1 if value is of specified type, 0 otherwise.
 */
int
xmmsv_is_type (const xmmsv_t *val, xmmsv_type_t t)
{
	x_api_error_if (!val, "NULL value", 0);

	return (xmmsv_get_type (val) == t);
}

/* Merely legacy aliases */

/**
 * Check if the value stores an error.
 *
 * @param val a #xmmsv_t
 * @return 1 if error was encountered, 0 otherwise.
 */
int
xmmsv_is_error (const xmmsv_t *val)
{
	return xmmsv_is_type (val, XMMSV_TYPE_ERROR);
}

/**
 * Retrieves an error string describing the server error from the
 * value.
 *
 * @param val a #xmmsv_t containing a integer.
 * @param r the return error.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_error (const xmmsv_t *val, const char **r)
{
	if (!val || val->type != XMMSV_TYPE_ERROR) {
		return 0;
	}

	*r = val->value.error;

	return 1;
}

/**
 * Retrieves a signed integer from the value.
 *
 * @param val a #xmmsv_t containing an integer.
 * @param r the return integer.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_int (const xmmsv_t *val, int32_t *r)
{
	if (!val || val->type != XMMSV_TYPE_INT32) {
		return 0;
	}

	*r = val->value.int32;

	return 1;
}

/**
 * Retrieves a string from the value.
 *
 * @param val a #xmmsv_t containing a string.
 * @param r the return string. This string is owned by the value and
 * will be freed when the value is freed.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_string (const xmmsv_t *val, const char **r)
{
	if (!val || val->type != XMMSV_TYPE_STRING) {
		return 0;
	}

	*r = val->value.string;

	return 1;
}

/**
 * Retrieves a collection from the value.
 *
 * @param val a #xmmsv_t containing a collection.
 * @param c the return collection. This collection is owned by the
 * value and will be unref'd when the value is freed.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_coll (const xmmsv_t *val, xmmsv_coll_t **c)
{
	if (!val || val->type != XMMSV_TYPE_COLL) {
		return 0;
	}

	*c = (xmmsv_coll_t *) val;

	return 1;
}

/**
 * Retrieves binary data from the value.
 *
 * @param val a #xmmsv_t containing a string.
 * @param r the return data. This data is owned by the value and will
 * be freed when the value is freed.
 * @param rlen the return length of data.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_bin (const xmmsv_t *val, const unsigned char **r, unsigned int *rlen)
{
	if (!val || val->type != XMMSV_TYPE_BIN) {
		return 0;
	}

	*r = val->value.bin.data;
	*rlen = val->value.bin.len;

	return 1;
}
