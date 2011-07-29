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

#include "ipc_call.h"

xmmsv_t *
__xmms_ipc_call (xmms_object_t *object, gint cmd, ...)
{
	xmms_object_cmd_arg_t arg;
	xmmsv_t *entry, *params;
	va_list ap;

	params = xmmsv_new_list ();

	va_start (ap, cmd);
	while ((entry = va_arg (ap, xmmsv_t *)) != NULL) {
		xmmsv_list_append (params, entry);
		xmmsv_unref (entry);
	}
	va_end (ap);

	xmms_object_cmd_arg_init (&arg);
	arg.args = params;

	xmms_object_cmd_call (XMMS_OBJECT (object), cmd, &arg);
	xmmsv_unref (params);

	if (xmms_error_isok (&arg.error)) {
		return arg.retval;
	}

	if (arg.retval != NULL) {
		xmmsv_unref (arg.retval);
	}

	return xmmsv_new_error (arg.error.message);
}
