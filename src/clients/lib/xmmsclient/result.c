/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient_ipc.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmmsc/xmmsc_errorcodes.h>
#include <xmmsc/xmmsc_stdint.h>
#include <xmmsc/xmmsc_stdbool.h>
#include <xmmscpriv/xmmsv_c2c.h>

typedef enum {
	XMMSC_RESULT_CALLBACK_DEFAULT,
	XMMSC_RESULT_CALLBACK_RAW,
	XMMSC_RESULT_CALLBACK_C2C
} xmmsc_result_callback_type_t;

typedef struct xmmsc_result_callback_St {
	xmmsc_result_callback_type_t type;
	xmmsc_result_notifier_t func;

	void *user_data;
	xmmsc_user_data_free_func_t free_func;
} xmmsc_result_callback_t;

static void xmmsc_result_restart (xmmsc_result_t *res);
static void xmmsc_result_notifier_add (xmmsc_result_t *res, xmmsc_result_callback_t *cb);
static void xmmsc_result_notifier_remove (xmmsc_result_t *res, x_list_t *node);
static void xmmsc_result_notifier_delete (xmmsc_result_t *res, x_list_t *node);
static void xmmsc_result_notifier_delete_all (xmmsc_result_t *res);

static xmmsc_result_callback_t *xmmsc_result_callback_new_default (xmmsc_result_notifier_t f, void *udata, xmmsc_user_data_free_func_t free_f);
static xmmsc_result_callback_t *xmmsc_result_callback_new_raw (xmmsc_result_notifier_t f, void *udata, xmmsc_user_data_free_func_t free_f);
static xmmsc_result_callback_t *xmmsc_result_callback_new_c2c (xmmsc_result_notifier_t f, void *udata, xmmsc_user_data_free_func_t free_f);

struct xmmsc_result_St {
	xmmsc_connection_t *c;

	/** refcounting */
	int ref;

	xmmsc_result_type_t type;

	/** notifiers */
	x_list_t *notifiers;

	xmmsc_ipc_t *ipc;

	bool parsed;
	bool is_c2c;

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
	x_return_if_fail (res);

	/* Free memory! */
	if (res->ipc) {
		xmmsc_ipc_result_unregister (res->ipc, res);
	}

	if (res->data) {
		xmmsv_unref (res->data);
	}

	xmmsc_result_notifier_delete_all (res);

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
 * Disconnect all notifiers for a signal or a broadcast result.
 * @param res The result to disconnect, must be of class signal or broadcast.
 */
void
xmmsc_result_disconnect (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	switch (res->type) {
		case XMMSC_RESULT_CLASS_SIGNAL:
		case XMMSC_RESULT_CLASS_BROADCAST:
		case XMMSC_RESULT_CLASS_DEFAULT:
			xmmsc_result_notifier_delete_all (res);
			break;
		default:
			x_api_error_if (1, "invalid result type",);
	}
}

static void
xmmsc_result_restart (xmmsc_result_t *res)
{
	x_return_if_fail (res);
	x_return_if_fail (res->c);

	if (res->type != XMMSC_RESULT_CLASS_SIGNAL) {
		x_api_warning ("result is not restartable");
		return;
	}

	res->cookie = xmmsc_write_signal_msg (res->c, res->restart_signal);
}

static bool
xmmsc_result_parse_msg (xmmsc_result_t *res, xmms_ipc_msg_t *msg)
{
	if (xmms_ipc_msg_get_cmd (msg) == XMMS_IPC_COMMAND_ERROR) {
		/* If special error msg, extract the error and save in result */
		const char *errstr;
		xmmsv_t *error;

		if (!xmms_ipc_msg_get_value (msg, &error)) {
			xmmsc_result_seterror (res, "No error value!");
		} else {
			if (!xmmsv_get_error (error, &errstr)) {
				xmmsc_result_seterror (res, "No error message!");
			} else {
				xmmsc_result_seterror (res, errstr);
			}

			xmmsv_unref (error);
		}

		res->parsed = true;
		return true;
	} else if (xmms_ipc_msg_get_value (msg, &res->data)) {
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

/**
 * Set a result to be a client-to-client result.
 */
void
xmmsc_result_c2c_set (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	res->is_c2c = true;
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

/* Macro magic to define the xmmsc_result_*_notifier_set. */
#define GEN_RESULT_NOTIFIER_SET_FUNC(type) \
void \
xmmsc_result_notifier_set_##type (xmmsc_result_t *res, \
                                  xmmsc_result_notifier_t func, \
                                  void *user_data) \
{ \
	xmmsc_result_notifier_set_##type##_full (res, func, user_data, NULL); \
}

/* And more macro magic to define the xmmsc_result_*_notifier_set_full. */
#define GEN_RESULT_NOTIFIER_SET_FULL_FUNC(type) \
void \
xmmsc_result_notifier_set_##type##_full (xmmsc_result_t *res, \
                                         xmmsc_result_notifier_t func, \
                                         void *user_data, \
                                         xmmsc_user_data_free_func_t free_func) \
{ \
	xmmsc_result_callback_t *cb; \
\
	x_return_if_fail (res); \
	x_return_if_fail (func); \
\
	cb = xmmsc_result_callback_new_##type (func, user_data, free_func); \
	xmmsc_result_notifier_add (res, cb); \
}

/**
 * Set up a default callback for the result retrieval. This callback
 * will be called when the answer arrives.
 * The callback receives the value as sent by the server or another
 * client, that is, for c2c messages only the payload is passed to the
 * callback.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 */

GEN_RESULT_NOTIFIER_SET_FUNC (default)

/**
 * Set up a default callback for the result retrieval. This callback
 * will be called when the answer arrives. This function differs from
 * xmmsc_result_default_notifier_set in the additional free_func parameter,
 * which allows to pass a pointer to a function which will be called
 * to free the user_data when needed.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 * @param free_func optional function that should be called to free the user_data
 */

GEN_RESULT_NOTIFIER_SET_FULL_FUNC (default)

/**
 * Set up a raw callback for the result retrieval. This callback
 * will be called when the answer arrives.
 * The client receives the value sent by the server or, for
 * client-to-client messages, the full message, whose fields can
 * be extracted with the appropriate functions.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 *
 * \sa xmmsv_c2c_message_get_payload and others.
 */

GEN_RESULT_NOTIFIER_SET_FUNC (raw)

/**
 * Set up a raw callback for the result retrieval. This callback
 * will be called when the answer arrives. This function differs from
 * xmmsc_result_raw_notifier_set in the additional free_func parameter,
 * which allows to pass a pointer to a function which will be called
 * to free the user_data when needed.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 * @param free_func optional function that should be called to free the user_data
 */

GEN_RESULT_NOTIFIER_SET_FULL_FUNC (raw)

/**
 * Set up a c2c callback for the result retrieval. This callback
 * will be called when the answer arrives.
 * This callback always receives values formatted as client-to-client
 * messages, whose fields can be extracted with the appropriate functions.
 * For values sent by the server, the sender id and message id fields
 * will be zero.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 *
 * \sa xmmsv_c2c_message_get_payload and others.
 */

GEN_RESULT_NOTIFIER_SET_FUNC (c2c)

/**
 * Set up a c2c callback for the result retrieval. This callback
 * will be called when the answer arrives. This function differs from
 * xmmsc_result_c2c_notifier_set in the additional free_func parameter,
 * which allows to pass a pointer to a function which will be called
 * to free the user_data when needed.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 * @param free_func optional function that should be called to free the user_data
 */

GEN_RESULT_NOTIFIER_SET_FULL_FUNC (c2c)

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
	x_return_if_fail (res->ipc);

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
 * Helper for xmmsc_result_run.
 * Correctly calls a callback based on its type.
 * @param res The result that owns the callback
 * @param cb The callback to be called
 * @return The result of the callback
 */
static int
xmmsc_result_run_callback (xmmsc_result_t *res, xmmsc_result_callback_t *cb)
{
	int ret;
	xmmsv_t *val = NULL;

	if (res->is_c2c && !xmmsv_is_error (res->data)) {
		if (cb->type == XMMSC_RESULT_CALLBACK_DEFAULT) {
			/* For default callbacks, pass the payload in c2c
			 * messages.
			 */
			val = xmmsv_ref (xmmsv_c2c_message_get_payload (res->data));
		}
	} else {
		if (cb->type == XMMSC_RESULT_CALLBACK_C2C) {
			/* For c2c callbacks and non-c2c messages, build a
			 * message with pseudo c2c metadata.
			 */
			val = xmmsv_c2c_message_format (0, 0, 0, res->data);
		}
	}

	if (!val) {
		val = xmmsv_ref (res->data);
	}

	ret = cb->func (val, cb->user_data);

	xmmsv_unref (val);
	return ret;
}

/**
 * @internal
 */
void
xmmsc_result_run (xmmsc_result_t *res, xmms_ipc_msg_t *msg)
{
	x_list_t *n, *next;
	xmmsc_result_callback_t *cb;

	x_return_if_fail (res);
	x_return_if_fail (msg);

	if (!xmmsc_result_parse_msg (res, msg)) {
		xmms_ipc_msg_destroy (msg);
		return;
	}

	xmms_ipc_msg_destroy (msg);

	xmmsc_result_ref (res);

	/* Run all notifiers and check for positive return values */
	n = res->notifiers;
	while (n) {
		int keep;

		next = x_list_next (n);
		cb = n->data;

		keep = xmmsc_result_run_callback (res, cb);
		if (!keep || res->type == XMMSC_RESULT_CLASS_DEFAULT) {
			xmmsc_result_notifier_delete (res, n);
		}

		n = next;
	}

	/* If this result is a signal, and we still have some notifiers
	 * we need to restart the signal.
	 */
	if (res->notifiers && res->type == XMMSC_RESULT_CLASS_SIGNAL) {
		/* We restart the signal using the same result. */
		xmmsc_result_restart (res);
	}

	if (res->type != XMMSC_RESULT_CLASS_DEFAULT && !res->is_c2c) {
		/* We keep the results alive with signals and broadcasts,
		   but we just renew the value because it went out of scope.
		   (freeing the payload, forget about it).
		   The exception being for c2c results of the BROADCAST class
		   (those associated with messages and replies), which may be
		   used synchronously as if they belonged to the DEFAULT class.
		*/
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

	res->c = c;

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

void
xmmsc_result_clear_weakrefs (xmmsc_result_t *result)
{
	result->c = NULL;
	result->ipc = NULL;
}

/* Macro-magically define the xmmsc_result_callback_new_* functions. */
#define GEN_RESULT_CALLBACK_NEW_FUNC(name, cbtype) \
static xmmsc_result_callback_t * \
xmmsc_result_callback_new_##name (xmmsc_result_notifier_t f, void *udata, \
                                  xmmsc_user_data_free_func_t free_f) \
{ \
	xmmsc_result_callback_t *cb; \
\
	cb = x_new0 (xmmsc_result_callback_t, 1); \
	if (!cb) { \
		x_oom(); \
		return NULL; \
	} \
\
	cb->type = cbtype; \
	cb->user_data = udata; \
	cb->free_func = free_f; \
	cb->func = f; \
\
	return cb; \
}

GEN_RESULT_CALLBACK_NEW_FUNC (default, XMMSC_RESULT_CALLBACK_DEFAULT)
GEN_RESULT_CALLBACK_NEW_FUNC (raw, XMMSC_RESULT_CALLBACK_RAW)
GEN_RESULT_CALLBACK_NEW_FUNC (c2c, XMMSC_RESULT_CALLBACK_C2C)

/* Add a new notifier to a result
 */
static void
xmmsc_result_notifier_add (xmmsc_result_t *res, xmmsc_result_callback_t *cb)
{
	/* The pending call takes one ref */
	xmmsc_result_ref (res);
	res->notifiers = x_list_append (res->notifiers, cb);
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

/* Dereference all notifiers from a result and delete their udata. */
static void
xmmsc_result_notifier_delete_all (xmmsc_result_t *res)
{
	x_list_t *n = res->notifiers;

	while (n) {
		x_list_t *next = x_list_next (n);
		xmmsc_result_notifier_delete (res, n);
		n = next;
	}

	res->notifiers = NULL;
}
