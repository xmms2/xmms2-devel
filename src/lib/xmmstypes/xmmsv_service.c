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

#include <xmmsc/xmmsv.h>
#include <xmmsc/xmmsc_util.h>
#include <xmmsc/xmmsv_service.h>
#include <xmmsclientpriv/xmmsclient_util.h>

/**
 * Create a new argument for a method.
 *
 * @param name The name of the argument. Must not be NULL.
 * @param docstring The docstring of the argument.
 * @param type The expected type of the argument. Use XMMSV_TYPE_NONE to
 * accept any type. XMMSV_TYPE_ERROR is reserved and should not be used.
 * @param default_value Value to set this argument to if it's missing in the
 * method call. Implies that the argument is optional. If NULL, the argument
 * is not optional.
 */
xmmsv_t *
xmmsv_sc_argument_new (const char *name, const char *docstring,
                       xmmsv_type_t type, xmmsv_t *default_value)
{
	xmmsv_t *arg;

	x_api_error_if (!name, "with NULL name.", NULL);
	x_api_error_if (type == XMMSV_TYPE_ERROR, "with ERROR type.", NULL);
	x_api_error_if (default_value && type != XMMSV_TYPE_NONE &&
	                xmmsv_get_type (default_value) != type,
	                "with wrong type for default value.", NULL);

	arg = xmmsv_new_dict ();
	if (!arg) {
		x_oom ();
		return NULL;
	}

	xmmsv_dict_set_string (arg, "name", name);
	xmmsv_dict_set_int (arg, "type", type);

	if (docstring) {
		xmmsv_dict_set_string (arg, "docstring", docstring);
	}

	if (default_value) {
		xmmsv_dict_set (arg, "default_value", default_value);
	}

	return arg;
}

/*
 * FIXME: The x_return_val_if_fail is wrong in this case
 * because data is sent by a remote client => not a programmatic error.
 */
#define GEN_SC_ARGUMENT_FIELD_GET_FUNC(field, getter, ctype, failval) \
ctype \
xmmsv_sc_argument_get_##field (xmmsv_t *arg) \
{ \
	ctype ret; \
	if (!getter (arg, #field, &ret)) \
		return failval; \
	return ret; \
}

/**
 * Extract the name of an argument.
 * @param arg The argument.
 *
 * @return The name upon success, NULL upon failure.
 */
GEN_SC_ARGUMENT_FIELD_GET_FUNC (name, xmmsv_dict_entry_get_string,
                                const char *, NULL);

/**
 * Extract the docstring of an argument.
 * @param arg The argument.
 *
 * @return The docstring upon success, NULL upon failure.
 */
GEN_SC_ARGUMENT_FIELD_GET_FUNC (docstring, xmmsv_dict_entry_get_string,
                                const char *, NULL);

/**
 * Extract the type of an argument.
 * @param arg The argument.
 *
 * @return The type, or XMMSV_TYPE_ERROR upon error.
 */
GEN_SC_ARGUMENT_FIELD_GET_FUNC (type, xmmsv_dict_entry_get_int, int64_t,
                                XMMSV_TYPE_ERROR);

/**
 * Extract the default value of an argument.
 * @param arg The argument.
 *
 * @return The default value or NULL upon error.
 */
GEN_SC_ARGUMENT_FIELD_GET_FUNC (default_value, xmmsv_dict_get, xmmsv_t *,
                                NULL);
