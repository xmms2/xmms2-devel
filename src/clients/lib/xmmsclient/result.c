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
#include "xmms/error_xmms.h"

#include "internal/xmmsclient_int.h"


struct xmmsc_result_St {
	int resultid;
	/* xmmsc_result_type_t type; */
	xmmsc_connection_t *conn;

	DBusPendingCall *dbus_call;
	
	/** Stored value */
	DBusMessage *reply;
	
	/** refcounting */
	int ref;

	/** notifiers */
	xmmsc_result_notifier_t func;
	void *user_data;

	int error;
	char *error_str;


	/** 
	 * signal to restore set by
	 * the send funciton 
	 * */
	char *restart_signal; 
};

void
xmmsc_result_ref (xmmsc_result_t *res)
{
	x_return_if_fail (res);
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

	/*
	if (res->reply)
		dbus_message_unref (res->reply);
	*/
	
	if (res->dbus_call)  {
		dbus_pending_call_unref (res->dbus_call);
	}

	free (res);
}

void
xmmsc_result_restartable (xmmsc_result_t *res, xmmsc_connection_t *conn, char *signal)
{
	x_return_if_fail (res);
	x_return_if_fail (conn);
	x_return_if_fail (signal);

	res->restart_signal = signal;
	res->conn = conn;

}

xmmsc_result_t *
xmmsc_result_restart (xmmsc_result_t *res)
{
	DBusMessageIter itr;
	DBusMessage *msg;
	xmmsc_result_t *ret;

	x_return_null_if_fail (res);
	x_return_null_if_fail (res->conn);
	
	if (!res->restart_signal) {
		return NULL;
	}

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_CLIENT, XMMS_DBUS_INTERFACE, XMMS_METHOD_ONCHANGE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, res->restart_signal);
	ret = xmmsc_send_on_change (res->conn, msg);
	dbus_message_unref (msg);

	xmmsc_result_notifier_set (ret, res->func, res->user_data);
	xmmsc_result_restartable (ret, res->conn, res->restart_signal);

	return ret;
	
}

void 
xmmsc_result_unref (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	res->ref--;
	if (res->ref == 0) {
		xmmsc_result_free (res);
	}
}

void
xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data)
{
	x_return_if_fail (res);
	x_return_if_fail (func);

	/* The pending call takes one ref */
	xmmsc_result_ref (res);

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

	if (res->func) {
		res->func (res, (void*)res->user_data);
	}

	dbus_pending_call_unref (pending);

}

xmmsc_result_t *
xmmsc_result_new (DBusPendingCall *pending)
{
	xmmsc_result_t *res;

	res = x_new0 (xmmsc_result_t, 1);

	res->dbus_call = pending;

	/* user must give this back */
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

int
xmmsc_result_get_playlist_change (xmmsc_result_t *res, 
				  unsigned int *change, 
				  unsigned int *id, 
				  unsigned int *argument)
{
	DBusMessageIter itr;

	x_return_val_if_fail (res, 0);

	dbus_message_iter_init (res->reply, &itr);

	if (dbus_message_iter_get_arg_type (&itr) != DBUS_TYPE_UINT32) {
		return 0;
	}
	*change = dbus_message_iter_get_uint32 (&itr);

	dbus_message_iter_next (&itr);
	if (dbus_message_iter_get_arg_type (&itr) != DBUS_TYPE_UINT32) {
		return 0;
	}

	*id = dbus_message_iter_get_uint32 (&itr);
	dbus_message_iter_next (&itr);
	if (dbus_message_iter_get_arg_type (&itr) != DBUS_TYPE_UINT32) {
		return 0;
	}

	*argument = dbus_message_iter_get_uint32 (&itr);

	return 1;
}

int
xmmsc_result_get_int (xmmsc_result_t *res, int *r)
{
	DBusMessageIter itr;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}
	
	dbus_message_iter_init (res->reply, &itr);
	if (dbus_message_iter_get_arg_type (&itr) != DBUS_TYPE_INT32) {
		return 0;
	}
	*r = dbus_message_iter_get_int32 (&itr);

	return 1;
}


int
xmmsc_result_get_uint (xmmsc_result_t *res, unsigned int *r)
{
	DBusMessageIter itr;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}
	
	dbus_message_iter_init (res->reply, &itr);
	if (dbus_message_iter_get_arg_type (&itr) != DBUS_TYPE_UINT32) {
		return 0;
	}
	*r = dbus_message_iter_get_uint32 (&itr);

	return 1;
}

int
xmmsc_result_get_string (xmmsc_result_t *res, char **r)
{
	DBusMessageIter itr;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}
	
	dbus_message_iter_init (res->reply, &itr);
	if (dbus_message_iter_get_arg_type (&itr) != DBUS_TYPE_STRING) {
		return 0;
	}
	*r = dbus_message_iter_get_string (&itr);

	return 1;
}

int
xmmsc_result_get_mediainfo (xmmsc_result_t *res, x_hash_t **r)
{
	DBusMessageIter itr;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	dbus_message_iter_init (res->reply, &itr);
	*r = xmmsc_deserialize_mediainfo (&itr);

	if (!*r) {
		printf ("snett snett\n");
		return 0;
	}

	return 1;
}


int
xmmsc_result_get_stringlist (xmmsc_result_t *res, x_list_t **r)
{
	DBusMessageIter itr;
	x_list_t *list = NULL;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	dbus_message_iter_init (res->reply, &itr);

	while (42) {
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
			list = x_list_append (list, dbus_message_iter_get_string (&itr));
		} else {
			list = NULL;
			break;
		}

		if (!dbus_message_iter_has_next (&itr))
			break;
		
		dbus_message_iter_next (&itr);

	}

	if (!list)
		return 0;

	*r = list;

	return 1;
}


int
xmmsc_result_get_uintlist (xmmsc_result_t *res, x_list_t **r)
{
	DBusMessageIter itr;
	x_list_t *list = NULL;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	dbus_message_iter_init (res->reply, &itr);

	while (42) {
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
			list = x_list_append (list, XUINT_TO_POINTER (dbus_message_iter_get_uint32 (&itr)));
		} else {
			list = NULL;
			break;
		}

		if (!dbus_message_iter_has_next (&itr))
			break;
		
		dbus_message_iter_next (&itr);

	}

	if (!list)
		return 0;

	*r = list;

	return 1;
}


int
xmmsc_result_get_intlist (xmmsc_result_t *res, x_list_t **r)
{
	DBusMessageIter itr;
	x_list_t *list = NULL;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	dbus_message_iter_init (res->reply, &itr);

	while (42) {
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_INT32) {
			list = x_list_append (list, XINT_TO_POINTER (dbus_message_iter_get_int32 (&itr)));
		} else {
			list = NULL;
			break;
		}

		if (!dbus_message_iter_has_next (&itr))
			break;
		
		dbus_message_iter_next (&itr);

	}

	if (!list)
		return 0;

	*r = list;

	return 1;
}

