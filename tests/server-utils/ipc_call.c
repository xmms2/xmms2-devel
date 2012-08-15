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

#include <stdio.h>

#include "ipc_call.h"
#include "xmmsc/xmmsc_util.h"

struct xmms_future_St {
	xmms_object_t *object;
	gint message;

	xmmsv_t *result;

	glong delay;
	glong timeout;

	GMutex *mutex;
	GCond *cond;
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

	g_mutex_lock (future->mutex);

	if (future->result != NULL) {
		xmmsv_list_append (future->result, val);
		g_cond_signal (future->cond);
	}

	g_mutex_unlock (future->mutex);
}

xmms_future_t *
__xmms_ipc_check_signal (xmms_object_t *object, gint message,
                         glong delay, glong timeout)
{
	xmms_future_t *future = g_new0 (xmms_future_t, 1);

	xmms_object_ref (object);

	future->result = xmmsv_new_list ();
	future->object = object;
	future->message = message;

	future->mutex = g_mutex_new ();
	future->cond = g_cond_new ();

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
	future->result = NULL;

	g_mutex_free (future->mutex);
	g_cond_free (future->cond);

	g_free (future);
}

/**
 * Wait until a requested number of signals have occurred.
 *
 * If the the number of signals does not occur within the
 * allotted time, a timeout will occur and it will be up
 * to the user of the API to check that the number of
 * signals returned match what was expected.
 *
 * @return max 'count' signals, ownership transferred.
 */
xmmsv_t *
xmms_future_await (xmms_future_t *future, gint count)
{
	xmmsv_t *result, *entry;
	GTimeVal timeout;
	gint i, entries;

	g_mutex_lock (future->mutex);

	g_get_current_time (&timeout);
	g_time_val_add (&timeout, future->timeout);

	while (xmmsv_list_get_size (future->result) < count) {
		GTimeVal now, wait;

		g_get_current_time (&now);
		if (now.tv_sec >= timeout.tv_sec && now.tv_usec > timeout.tv_usec) {
			break;
		}

		g_get_current_time (&wait);
		g_time_val_add (&wait, future->delay);

		g_cond_timed_wait (future->cond, future->mutex, &wait);
	}

	result = xmmsv_new_list ();

	entries = MIN (count, xmmsv_list_get_size (future->result));
	for (i = 0; i < entries; i++) {
		xmmsv_list_get (future->result, 0, &entry);
		xmmsv_list_append (result, entry);
		xmmsv_list_remove (future->result, 0);
	}

	g_mutex_unlock (future->mutex);

	return result;
}
