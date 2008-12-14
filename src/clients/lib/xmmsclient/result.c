/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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
#include <sys/types.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_errorcodes.h"
#include "xmmsc/xmmsc_stdint.h"
#include "xmmsc/xmmsc_strlist.h"
#include "xmmsc/xmmsc_stdbool.h"

xmmsc_result_t *xmmsc_result_restart (xmmsc_result_t *res);
static void xmmsc_result_notifier_remove (xmmsc_result_t *res, x_list_t *node);
static void xmmsc_result_notifier_delete (xmmsc_result_t *res, x_list_t *node);

typedef struct xmmsc_result_callback_St {
	xmmsc_result_notifier_t func;
	void *user_data;
	xmmsc_user_data_free_func_t free_func;
} xmmsc_result_callback_t;

static xmmsc_result_callback_t *xmmsc_result_callback_new (xmmsc_result_notifier_t f, void *udata, xmmsc_user_data_free_func_t free_f);

struct xmmsc_result_St {
	xmmsc_connection_t *c;

	/** refcounting */
	int ref;

	xmmsc_result_type_t type;

	/** notifiers */
	x_list_t *notifiers;

	xmmsc_ipc_t *ipc;

	bool parsed;

	uint32_t cookie;
	uint32_t restart_signal;

	xmmsv_t *data;

	xmmsc_visualization_t *visc;
};

/**
 * @defgroup Result Result
 * @brief Result manipulation and error handling
 * @ingroup XMMSClient
 *
 * Each command to the server will return a #xmmsc_result_t
 * to the programmer. This object will be used to see the results
 * off the call. It will handle errors and the results.
 *
 * results could be used in both sync and async fashions. Here
 * is a sync example:
 * @code
 * xmmsc_result_t *res;
 * xmmsv_t *val;
 * uint32_t id;
 * res = xmmsc_playback_get_current_id (connection);
 * xmmsc_result_wait (res);
 * if (!val = xmmsc_result_get_value (res)) {
 *   printf ("error: failed to retrieve value!");
 * }
 * if (xmmsv_is_error (val)) {
 *   printf ("error: %s", xmmsv_get_error (val));
 * }
 * xmmsv_get_uint (val, &id);
 * xmmsc_result_unref (res);
 * printf ("current id is: %d", id);
 * @endcode
 *
 * an async example is a bit more complex...
 * @code
 * static void handler (xmmsv_t *val, void *userdata) {
 *   uint32_t id;
 *   if (xmmsv_is_error (val)) {
 *      printf ("error: %s", xmmsv_get_error (val));
 *   }
 *   xmmsv_get_uint (val, &id);
 *   printf ("current id is: %d", id);
 * }
 *
 * int main () {
 *   // Connect blah blah ...
 *   xmmsc_result_t *res;
 *   res = xmmsc_playback_get_current_id (connection);
 *   xmmsc_result_notifier_set (res, handler);
 *   xmmsc_result_unref (res);
 * }
 * @endcode
 * When the answer arrives handler will be called. with the resulting #xmmsv_t
 * @{
**/

/**
 * References the #xmmsc_result_t
 *
 * @param result the result to reference.
 * @return result
 */
xmmsc_result_t *
xmmsc_result_ref (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, NULL);
	res->ref++;

	return res;
}

/**
 * @todo Deallocate all types here
 */
static void
xmmsc_result_free (xmmsc_result_t *res)
{
	x_list_t *n, *next;

	x_return_if_fail (res);

	/* Free memory! */

	xmmsc_ipc_result_unregister (res->ipc, res);

	xmmsc_unref (res->c);

	if (res->data) {
		xmmsv_unref (res->data);
	}

	n = res->notifiers;
	while (n) {
		next = x_list_next (n);
		xmmsc_result_notifier_delete (res, n);
		n = next;
	}

	free (res);
}

/**
 * Get the class of the result (default, signal, broadcast).
 * @returns The class of the result of type #xmmsc_result_type_t
 */
xmmsc_result_type_t
xmmsc_result_get_class (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, XMMSC_RESULT_CLASS_DEFAULT);

	return res->type;
}

/**
 * Disconnect a signal or a broadcast.
 * @param res The result to disconnect, must be of class signal or broadcast.
 */
void
xmmsc_result_disconnect (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	switch (res->type) {
		case XMMSC_RESULT_CLASS_SIGNAL:
		case XMMSC_RESULT_CLASS_BROADCAST:
			xmmsc_result_unref (res);
			break;
		default:
			x_api_error_if (1, "invalid result type",);
	}
}

/**
 * A lot of signals you would like to get notified about
 * when they change, instead of polling the server all the time.
 * This results are "restartable".
 * Here is an example on how you use a restartable signal.
 * @code
 * static void handler (xmmsc_result_t *res, void *userdata) {
 *   uint32_t id;
 *   xmmsc_result_t *newres;
 *
 *   if (xmmsc_result_iserror) {
 *      printf ("error: %s", xmmsc_result_get_error (res);
 *   }
 *
 *   xmmsc_result_get_uint (res, &id);
 *   newres = xmmsc_result_restart (res); // this tells the server to send updates to the SAME function again.
 *   xmmsc_result_unref (res);
 *   xmmsc_result_unref (newres);
 *   printf ("current id is: %d", id);
 * }
 *
 * int main () {
 *   // Connect blah blah ...
 *   xmmsc_result_t *res;
 *   res = xmmsc_signal_playback_playtime (connection);
 *   xmmsc_result_notifier_set (res, handler);
 *   xmmsc_result_unref (res);
 * }
 * @endcode
 * In the above example the handler would be called when the playtime is updated.
 * Only signals are restatable. Broadcasts will automaticly restart.
 */
xmmsc_result_t *
xmmsc_result_restart (xmmsc_result_t *res)
{
	xmmsc_result_t *newres;
	xmms_ipc_msg_t *msg;
	int num_notifiers;

	x_return_null_if_fail (res);
	x_return_null_if_fail (res->c);

	x_api_error_if (res->type != XMMSC_RESULT_CLASS_SIGNAL,
	                "result is not restartable", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);
	xmms_ipc_msg_put_uint32 (msg, res->restart_signal);

	newres = xmmsc_send_msg (res->c, msg);

	/* The result's notifiers are moved over to newres. */
	if (newres->notifiers) {
		x_internal_error ("restart result's notifiers non-empty!");
	}

	newres->notifiers = res->notifiers;
	res->notifiers = NULL;

	/* Each pending call takes one ref */
	num_notifiers = x_list_length (newres->notifiers);

	newres->ref += num_notifiers;
	res->ref -= num_notifiers;

	xmmsc_result_restartable (newres, res->restart_signal);

	return newres;
}

static bool
xmmsc_result_parse_msg (xmmsc_result_t *res, xmms_ipc_msg_t *msg)
{
	if (xmms_ipc_msg_get_cmd (msg) == XMMS_IPC_CMD_ERROR) {
		/* If special error msg, extract the error and save in result */
		char *errstr;
		uint32_t len;

		if (!xmms_ipc_msg_get_string_alloc (msg, &errstr, &len)) {
			xmmsc_result_seterror (res, "No errormsg!");
		} else {
			xmmsc_result_seterror (res, errstr);
			free (errstr);
		}

		res->parsed = true;
		return true;
	} else if (xmms_ipc_msg_get_value_alloc (msg, &res->data)) {
		/* Expected message data retrieved! */
		res->parsed = true;
		return true;
	} else {
		/* FIXME: shouldn't parsed be false then? */
		return false;
	}
}


/**
 * return the cookie of a resultset.
 */
uint32_t
xmmsc_result_cookie_get (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, 0);

	return res->cookie;
}

void
xmmsc_result_visc_set (xmmsc_result_t *res, xmmsc_visualization_t *visc)
{
	x_return_if_fail (res);
	x_return_if_fail (!res->visc);
	res->visc = visc;
}

xmmsc_visualization_t *
xmmsc_result_visc_get (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, NULL);
	x_return_val_if_fail (res->visc, NULL);
	return res->visc;
}

xmmsc_connection_t *
xmmsc_result_get_connection (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, NULL);
	x_return_val_if_fail (res->c, NULL);
	return res->c;
}

/**
 * Decreases the references for the #xmmsc_result_t
 * When the number of references reaches 0 it will
 * be freed. And thus all data you extracted from it
 * will be deallocated.
 */

void
xmmsc_result_unref (xmmsc_result_t *res)
{
	x_return_if_fail (res);
	x_api_error_if (res->ref < 1, "with a freed result",);

	res->ref--;
	if (res->ref == 0) {
		xmmsc_result_free (res);
	}
}

/**
 * Set up a callback for the result retrival. This callback
 * Will be called when the answers arrives.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 */

void
xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data)
{
	xmmsc_result_notifier_set_full (res, func, user_data, NULL);
}

/**
 * Set up a callback for the result retrieval. This callback
 * will be called when the answer arrives. This function differs from
 * xmmsc_result_notifier_set in the additional free_func parameter,
 * which allows to pass a pointer to a function which will be called
 * to free the user_data when needed.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 * @param free_func optional function that should be called to free the user_data
 */

void
xmmsc_result_notifier_set_full (xmmsc_result_t *res, xmmsc_result_notifier_t func,
                                void *user_data, xmmsc_user_data_free_func_t free_func)
{
	xmmsc_result_callback_t *cb;

	x_return_if_fail (res);
	x_return_if_fail (func);

	/* The pending call takes one ref */
	xmmsc_result_ref (res);

	cb = xmmsc_result_callback_new (func, user_data, free_func);
	res->notifiers = x_list_append (res->notifiers, cb);
}


/**
 * Block for the reply. In a synchronous application this
 * can be used to wait for the result. Will return when
 * the server replyed.
 */

void
xmmsc_result_wait (xmmsc_result_t *res)
{
	const char *err = NULL;
	x_return_if_fail (res);

	while (!res->parsed && !(err = xmmsc_ipc_error_get (res->ipc))) {
		xmmsc_ipc_wait_for_event (res->ipc, 5);
	}

	if (err) {
		/* FIXME: xmmsv_unref (res->data) or not allocated ? */
		res->data = xmmsv_new_error (err);
	}
}

/**
 * @defgroup ResultValueRetrieval ResultValueRetrieval
 * @ingroup Result
 * @brief Explains how you can retrive values from a #xmmsc_result_t
 * @{
 */

/**
 * Get the value from a result. The reference is still owned by the
 * result.
 *
 * @param res a #xmmsc_result_t containing the value.
 * @returns A borrowed reference to the value received by the result.
 */
xmmsv_t *
xmmsc_result_get_value (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, NULL);
	x_return_val_if_fail (res->parsed, NULL);

	return res->data;
}

/** @} */

/** @} */

/** @internal */

/* Kept as a proxy for external use */
void
xmmsc_result_seterror (xmmsc_result_t *res, const char *errstr)
{
	if (res->data) {
		xmmsv_unref (res->data);
	}
	res->data = xmmsv_new_error (errstr);
}

void
xmmsc_result_restartable (xmmsc_result_t *res, uint32_t signalid)
{
	x_return_if_fail (res);

	res->restart_signal = signalid;
}

/**
 * @internal
 */
void
xmmsc_result_run (xmmsc_result_t *res, xmms_ipc_msg_t *msg)
{
	x_list_t *n, *next;
	int cmd;
	xmmsc_result_t *restart_res;
	xmmsc_result_callback_t *cb;

	x_return_if_fail (res);
	x_return_if_fail (msg);

	if (!xmmsc_result_parse_msg (res, msg)) {
		xmms_ipc_msg_destroy (msg);
		return;
	}

	cmd = xmms_ipc_msg_get_cmd (msg);

	xmms_ipc_msg_destroy (msg);

	xmmsc_result_ref (res);

	/* Run all notifiers and check for positive return values */
	n = res->notifiers;
	while (n) {
		next = x_list_next (n);
		cb = n->data;

		if (!cb->func (res->data, cb->user_data)) {
			xmmsc_result_notifier_delete (res, n);
		}

		n = next;
	}

	/* If this result is a signal, and we still have some notifiers
	 * we need to restart the signal.
	 */
	if (res->notifiers && cmd == XMMS_IPC_CMD_SIGNAL) {
		/* Note that xmmsc_result_restart will transfer ownership
		 * of the notifiers from the current result to the
		 * restarted one, so we don't need to fiddle with the
		 * notifiers here anymore.
		 */
		restart_res = xmmsc_result_restart (res);

		xmmsc_result_unref (restart_res);
	}

	if (cmd == XMMS_IPC_CMD_BROADCAST) {
		/* We keep the results alive with broadcasts, but we
		   just renew the value because it went out of scope.
		   (freeing the payload, forget about it) */
		xmmsv_unref (res->data);
		res->data = NULL;
	}

	xmmsc_result_unref (res);
}

/**
 * Allocates new #xmmsc_result_t and references it.
 * Should not be used from a client.
 * @internal
 */

xmmsc_result_t *
xmmsc_result_new (xmmsc_connection_t *c, xmmsc_result_type_t type,
                  uint32_t cookie)
{
	xmmsc_result_t *res;

	res = x_new0 (xmmsc_result_t, 1);
	if (!res) {
		x_oom ();
		return NULL;
	}

	res->c = xmmsc_ref (c);

	res->data = NULL;
	res->type = type;
	res->cookie = cookie;

	/* user must give this back */
	xmmsc_result_ref (res);

	/* Add it to the loop */
	xmmsc_ipc_result_register (c->ipc, res);

	/* For the destroy func */
	res->ipc = c->ipc;

	return res;
}


static xmmsc_result_callback_t *
xmmsc_result_callback_new (xmmsc_result_notifier_t f, void *udata,
                           xmmsc_user_data_free_func_t free_f)
{
	xmmsc_result_callback_t *cb;

	cb = x_new0 (xmmsc_result_callback_t, 1);
	cb->func = f;
	cb->user_data = udata;
	cb->free_func = free_f;

	return cb;
}

/* Dereference a notifier from a result.
 * The #x_list_t node containing the notifier is passed.
 */
static void
xmmsc_result_notifier_remove (xmmsc_result_t *res, x_list_t *node)
{
	free (node->data); /* remove the callback struct, but not the udata */
	res->notifiers = x_list_delete_link (res->notifiers, node);
	xmmsc_result_unref (res); /* each cb has a reference to res */
}

/* Dereference a notifier from a result and delete its udata.
 * The #x_list_t node containing the notifier is passed.
 */
static void
xmmsc_result_notifier_delete (xmmsc_result_t *res, x_list_t *node)
{
	xmmsc_result_callback_t *cb = node->data;

	/* remove the udata */
	if (cb->free_func) {
		cb->free_func (cb->user_data);
	}
	xmmsc_result_notifier_remove (res, node);
}
