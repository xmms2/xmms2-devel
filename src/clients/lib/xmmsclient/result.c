/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>

#include <pwd.h>
#include <sys/types.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>

#include "internal/xhash-int.h"
#include "internal/xlist-int.h"

#include "xmms/xmmsclient.h"
#include "xmms/signal_xmms.h"

#include "internal/xmmsclient_int.h"


struct xmmsc_result_St {
	int resultid;
	/* xmmsc_result_type_t type; */

	DBusPendingCall *dbus_call;
	
	/* Stored value */
	DBusMessage *reply;
	
	/* refcounting */
	int ref;

	/* notifiers */
	xmmsc_result_notifier_t func;
	void *user_data;

	int error;
	char *error_str;
};


void
xmmsc_result_ref (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	dbus_pending_call_ref (res->dbus_call);
	res->ref++;
}

/**
 * @todo Deallocate all types here
 */
static void
xmmsc_result_free (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	if (res->error_str)
		free (res->error_str);

	if (res->reply)
		dbus_message_unref (res->reply);

	free (res);
}


void 
xmmsc_result_unref (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	res->ref--;
	dbus_pending_call_unref (res->dbus_call);
	if (res->ref == 0) {
		xmmsc_result_free (res);
	}
}

void
xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data)
{
	x_return_if_fail (res);
	x_return_if_fail (func);

	res->func = func;
	res->user_data = user_data;
}

static void
xmmsc_result_pending_notifier (DBusPendingCall *pending, void *user_data)
{
	DBusMessage *reply;
	DBusError err;
	xmmsc_result_t *res = user_data;

	x_return_if_fail (pending);
	x_return_if_fail (user_data);

	if (!(reply = dbus_pending_call_get_reply (pending))) {
		fprintf (stderr, "This shouldn't really happen, something in DBUS is VERY VERY WRONG!\n");
		res->error = XMMS_ERROR_API_UNRECOVERABLE;
		return;
	}

	res->reply = reply;
	dbus_error_init (&err);
	if (dbus_set_error_from_message (&err, res->reply)) {
		res->error = 1; /* @todo add real error */
		res->error_str = strdup (err.message);
	}
	dbus_message_ref (res->reply);

	if (res->func) {
		res->func (res, (void*)res->user_data);
	}

	/* the pending call returns one ref */
	xmmsc_result_unref (res);
}

xmmsc_result_t *
xmmsc_result_new (DBusPendingCall *pending)
{
	xmmsc_result_t *res;

	res = x_new0 (xmmsc_result_t, 1);

	res->dbus_call = pending;

	/* user must give this back */
	xmmsc_result_ref (res);

	/* The pending call takes one ref */
	xmmsc_result_ref (res);
	dbus_pending_call_set_notify (res->dbus_call, 
				      xmmsc_result_pending_notifier, 
				      res, 
				      NULL);
	return res;
}


void
xmmsc_result_wait (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	dbus_pending_call_block (res->dbus_call);
}
				

/* value retrivial */

int
xmmsc_result_iserror (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, 1);

	if (res->error > 0) {
		return 1;
	}

	return 0;
}

const char *
xmmsc_result_get_error (xmmsc_result_t *res)
{
	x_return_null_if_fail (res);

	return res->error_str;
}

static int
result_check_sanity (xmmsc_result_t *res, DBusMessageIter *itr)
{
	x_return_val_if_fail (res, 0);

	if (res->error)
		return 0;

	if (!res->reply)
		return 0;

	dbus_message_iter_init (res->reply, itr);

	return 1;
}

int
xmmsc_result_get_int (xmmsc_result_t *res)
{
	DBusMessageIter itr;
	
	if (!result_check_sanity (res, &itr)) {
		res->error = XMMS_ERROR_API_RESULT_NOT_SANE;
		return 0;
	}
	
	return dbus_message_iter_get_int32 (&itr);
}

unsigned int
xmmsc_result_get_uint (xmmsc_result_t *res)
{
	DBusMessageIter itr;

	if (!result_check_sanity (res, &itr)) {
		res->error = XMMS_ERROR_API_RESULT_NOT_SANE;
		return 0;
	}
		
	return dbus_message_iter_get_uint32 (&itr);
}

char *
xmmsc_result_get_string (xmmsc_result_t *res)
{
	DBusMessageIter itr;
	if (!result_check_sanity (res, &itr)) {
		res->error = XMMS_ERROR_API_RESULT_NOT_SANE;
		return NULL;
	}

	return dbus_message_iter_get_string (&itr);
}

x_list_t *
xmmsc_result_get_list (xmmsc_result_t *res)
{
	DBusMessageIter itr;
	x_list_t *list = NULL;
	
	if (!result_check_sanity (res, &itr)) {
		res->error = XMMS_ERROR_API_RESULT_NOT_SANE;
		return NULL;
	}

	while (42) {
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
			list = x_list_append (list, dbus_message_iter_get_string (&itr));
		}

		if (!dbus_message_iter_has_next (&itr))
			break;
		
		dbus_message_iter_next (&itr);

	}

	return list;
}


unsigned int *
xmmsc_result_get_uint_array (xmmsc_result_t *res)
{
	DBusMessageIter itr;
	unsigned int *arr = NULL;

	if (!result_check_sanity (res, &itr)) {
		res->error = XMMS_ERROR_API_RESULT_NOT_SANE;
		return NULL;
	}


	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
		unsigned int len = dbus_message_iter_get_uint32 (&itr);
		if (len > 0) {
			int len;
			unsigned int *tmp;

			dbus_message_iter_next (&itr);
			dbus_message_iter_get_uint32_array (&itr, &tmp, &len);

			arr = malloc (sizeof (unsigned int) * (len + 1));
			memcpy (arr, tmp, len * sizeof(unsigned int));
			arr[len] = '\0';
					
		}
	}

	return arr;
}

double *
xmmsc_result_get_double_array (xmmsc_result_t *res)
{
	DBusMessageIter itr;
	int len;
	double *arr, *tmp;

	if (!result_check_sanity (res, &itr)) {
		res->error = XMMS_ERROR_API_RESULT_NOT_SANE;
		return NULL;
	}

	dbus_message_iter_get_double_array (&itr, &tmp, &len);

	arr = malloc (sizeof (double) * (len + 1));
	memcpy (arr, tmp, len * sizeof(double));
	arr[len] = '\0';

	return arr;
}

x_hash_t *
xmmsc_result_get_mediainfo (xmmsc_result_t *res) 
{
	DBusMessageIter itr;

	if (!result_check_sanity (res, &itr)) {
		res->error = XMMS_ERROR_API_RESULT_NOT_SANE;
		return NULL;
	}

	return xmmsc_deserialize_mediainfo (&itr);

}
