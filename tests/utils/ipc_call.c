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

#include "ipc_call.h"
#include "xmmsc/xmmsc_util.h"

struct xmms_future_St {
	xmmsv_t *result;
	xmms_object_t *object;
	gint message;
	gint delay;
	gint timeout;
};

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

static void
future_callback (xmms_object_t *object, xmmsv_t *val, gpointer udata)
{
	xmms_future_t *future = (xmms_future_t *) udata;

	xmmsv_list_append (future->result, val);
}

xmms_future_t *
__xmms_ipc_check_signal (xmms_object_t *object, gint message,
                         gint delay, gint timeout)
{
	xmms_future_t *future = g_new0 (xmms_future_t, 1);

	xmms_object_ref (object);

	future->result = xmmsv_new_list ();
	future->object = object;
	future->message = message;

	future->delay = delay;
	future->timeout = timeout;

	xmms_object_connect (future->object, future->message,
	                     future_callback, future);

	return future;
}

void
xmms_future_free (xmms_future_t *future)
{
	xmms_object_disconnect (future->object, future->message,
	                        future_callback, future);
	xmms_object_unref (future->object);
	xmmsv_unref (future->result);
	g_free (future);
}

/**
 * Fetch one signal result.
 *
 * Waits for one entry to enter the result list and then clears
 * the list and return that entry.
 *
 * TODO: Should check result, wait X ms, try again, repeat all
 *       until Y ms has passed.
 *
 * @return the first signal in queue, or NULL on error, ownership transferred.
 */
xmmsv_t *
xmms_future_await_one (xmms_future_t *future)
{
	xmmsv_t *entry;

	if (future->result == NULL) {
		return NULL;
	}

	if (!xmmsv_list_get (future->result, 0, &entry)) {
		fprintf (stderr, "Expected one element, found none\n");
		xmms_dump_stack ();
		return NULL;
	}

	xmmsv_ref (entry);

	xmmsv_list_remove (future->result, 0);

	return entry;
}

/**
 * Fetch one signal result.
 *
 * Waits until a certain number of signal results have
 * been cought, then creates a new result list and
 * returns the signals cought.
 *
 * TODO: Should check result, wait X ms, try again, repeat all
 *       until Y ms has passed.
 *
 * @return all signals in queue, or NULL on error, ownership transferred.
 */
xmmsv_t *
xmms_future_await_many (xmms_future_t *future, gint count)
{
	xmmsv_t *result;

	if (future->result == NULL) {
		return NULL;
	}

	if (xmmsv_list_get_size (future->result) != count) {
		fprintf (stderr, "Expected size %d != %d\n",
		         count, xmmsv_list_get_size (future->result));
		xmms_dump_stack ();
		return NULL;
	}

	result = future->result;

	future->result = xmmsv_new_list ();

	return result;
}
