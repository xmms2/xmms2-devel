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
#include <stdint.h>

#include <pwd.h>
#include <sys/types.h>

#include "xmms/ipc.h"
#include "xmms/ipc_msg.h"
#include "internal/xhash-int.h"
#include "internal/xlist-int.h"
#include "internal/client_ipc.h"

#include "xmms/xmmsclient.h"
#include "xmms/signal_xmms.h"
#include "xmms/error_xmms.h"

#include "internal/xmmsclient_int.h"


struct xmmsc_result_St {
	xmmsc_connection_t *c;

	/** refcounting */
	int ref;

	/** notifiers */
	xmmsc_result_notifier_t func;
	void *user_data;

	int error;
	char *error_str;

	uint32_t cid;

	/** reply */
	xmms_ipc_msg_t *reply;

	uint32_t restart_signal;

	xmmsc_ipc_t *ipc;
};

/**
 * @defgroup XMMSC_Result XMMSC_Result
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
 * unsigned int id;
 * res = xmmsc_playback_get_current_id (connection);
 * xmmsc_result_wait (res);
 * if (xmmsc_result_iserror) {
 *   printf ("error: %s", xmmsc_result_get_error (res);
 * }
 * xmmsc_result_get_uint (res, &id);
 * xmmsc_result_unref (res);
 * printf ("current id is: %d", id);
 * @endcode
 *
 * an async example is a bit more complex...
 * @code
 * static void handler (xmmsc_result_t *res, void *userdata) {
 *   unsigned int id;
 *   if (xmmsc_result_iserror) {
 *      printf ("error: %s", xmmsc_result_get_error (res);
 *   }
 *   xmmsc_result_get_uint (res, &id);
 *   xmmsc_result_unref (res);
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
 * When the answer arrives #handler will be called. with the resulting #xmmsc_result_t
 * @{
**/

/**
 * References the #xmmsc_result_t
 */
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

	xmmsc_unref (res->c);

	if (res->error_str)
		free (res->error_str);

	if (res->reply)
		xmms_ipc_msg_destroy (res->reply);

	xmmsc_ipc_result_unregister (res->ipc, res);
	
	free (res);
}

/**
 * @defgroup RestartableResults RestartableResults
 * @brief Covers Restartable #xmmsc_result_t's
 * @ingroup XMMSC_Result
 * A lot of signals you would like to get notified about
 * when they change, instead of polling the server all the time.
 * This results are "restartable".
 * Here is an example on how you use a restartable signal.
 * @code
 * static void handler (xmmsc_result_t *res, void *userdata) {
 *   unsigned int id;
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
 *   res = xmmsc_playback_get_current_id (connection);
 *   xmmsc_result_notifier_set (res, handler);
 *   xmmsc_result_unref (res);
 * }
 * @endcode
 * In the above example the #handler would be called when the id changes.
 * Not all commands are restartable, please check the documentation for
 * each function to see if it's restartable.
 * @{
 */

/** @internal */
void
xmmsc_result_restartable (xmmsc_result_t *res, uint32_t signalid)
{
	x_return_if_fail (res);

	res->restart_signal = signalid;
}

xmmsc_result_t *
xmmsc_result_restart (xmmsc_result_t *res)
{
	xmmsc_result_t *newres;
	xmms_ipc_msg_t *msg;

	x_return_null_if_fail (res);
	x_return_null_if_fail (res->c);


	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);
	xmms_ipc_msg_put_uint32 (msg, res->restart_signal);
	
	newres = xmmsc_send_msg (res->c, msg);
	xmms_ipc_msg_destroy (msg);
	
	xmmsc_result_notifier_set (newres, res->func, res->user_data);
	xmmsc_result_restartable (newres, res->restart_signal);
	
	return newres;
}

void
xmmsc_result_run (xmmsc_result_t *res, xmms_ipc_msg_t *msg)
{
	x_return_if_fail (res);
	x_return_if_fail (msg);

	res->reply = msg;
	
	xmmsc_result_ref (res);

	if (res->func) {
		res->func (res, res->user_data);
	}

	if (res->reply && res->reply->cmd == XMMS_IPC_CMD_BROADCAST) {
		/* Post-cleanup of broadcast callback
		 * User have to make sure that he DOESN'T
		 * save the resultset somewhere! */
		xmms_ipc_msg_destroy (res->reply);
		res->reply = NULL;
	}

	xmmsc_result_unref (res);
}

int
xmmsc_result_cid (xmmsc_result_t *res)
{
	if (!res)
		return 0;

	return res->cid;
}

/** @} */


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
	x_return_if_fail (res);
	x_return_if_fail (func);

	/* The pending call takes one ref */
	xmmsc_result_ref (res);

	res->func = func;
	res->user_data = user_data;
}

/**
 * Allocates new #xmmsc_result_t and refereces it.
 * Should not be used from a client.
 * @internal
 */

xmmsc_result_t *
xmmsc_result_new (xmmsc_connection_t *c, guint32 commandid)
{
	xmmsc_result_t *res;

	res = x_new0 (xmmsc_result_t, 1);

	res->c = c;
	xmmsc_ref (c);

	res->cid = commandid;

	/* user must give this back */
	xmmsc_result_ref (res);

	/* Add it to the loop */
	xmmsc_ipc_result_register (c->ipc, res);

	/* For the destroy func */
	res->ipc = c->ipc;

	return res;
}


/**
 * Block for the reply
 */

void
xmmsc_result_wait (xmmsc_result_t *res)
{
	x_return_if_fail (res);
	while (!res->reply) {
		xmmsc_ipc_wait_for_event (res->ipc, 5);
	}
}
				

/**
 * @defgroup ResultValueRetivial ResultValueRetivial 
 * @ingroup XMMSC_Result
 * @brief Explains how you can retrive values from a #xmmsc_result_t
 * @{
 */

void
xmmsc_result_seterror (xmmsc_result_t *res, char *errstr)
{
	res->error_str = errstr;
	res->error = 1;
}

/**
 * Check the #xmmsc_result_t for error.
 * @returns 1 if error was encountered, else 0
 */

int
xmmsc_result_iserror (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, 1);

	if (res->error > 0) {
		return 1;
	}

	return 0;
}

/**
 * Get an error string describing the error that occoured
 */ 

const char *
xmmsc_result_get_error (xmmsc_result_t *res)
{
	x_return_null_if_fail (res);

	return res->error_str;
}

/**
 * Retrieves a playlist change. This could be called on a
 * result from #xmmsc_playlist_change
 *
 * @param res a #xmmsc_result_t containing a playlist change.
 * @param change the type of change that occoured, possible changes are listed above
 * @param id the id in the playlist that where affected (if applicable)
 * @param argument optional argument to the change
 * @returns 1 if there was a playlist change in this #xmmsc_result_t
 */

int
xmmsc_result_get_playlist_change (xmmsc_result_t *res, 
				  unsigned int *change, 
				  unsigned int *id, 
				  unsigned int *argument)
{

	x_return_val_if_fail (res, 0);

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	if (!xmms_ipc_msg_get_uint32 (res->reply, change))
		return 0;
	if (!xmms_ipc_msg_get_uint32 (res->reply, id))
		return 0;
	if (!xmms_ipc_msg_get_uint32 (res->reply, argument))
		return 0;
	
	return 1;
}

/**
 * Retrives a signed integer from the resultset.
 * @param res a #xmmsc_result_t containing a integer.
 * @param r the return integer.
 * @ret 1 upon success otherwise 0
 */

int
xmmsc_result_get_int (xmmsc_result_t *res, int *r)
{
	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	if (!xmms_ipc_msg_get_int32 (res->reply, r))
		return 0;
	
	return 1;
}

/**
 * Retrives a unsigned integer from the resultset.
 * @param res a #xmmsc_result_t containing a integer.
 * @param r the return integer.
 * @ret 1 upon success otherwise 0
 */

int
xmmsc_result_get_uint (xmmsc_result_t *res, unsigned int *r)
{
	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	if (!xmms_ipc_msg_get_uint32 (res->reply, r))
		return 0;
	
	return 1;
}

/**
 * Retrives a string from the resultset.
 * @param res a #xmmsc_result_t containing a string.
 * @param r the return string.
 * @ret 1 upon success otherwise 0
 */

int
xmmsc_result_get_string (xmmsc_result_t *res, char **r)
{
	int len;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	if (!xmms_ipc_msg_get_string_alloc (res->reply, r, &len))
		return 0;
	
	return 1;
}

/**
 * Retrives a #x_hash_t containing the mediainfo from the resultset.
 * @param res a #xmmsc_result_t containing the mediainfo.
 * @param r the return #x_hash_t.
 * @ret 1 upon success otherwise 0
 */

int
xmmsc_result_get_hashtable (xmmsc_result_t *res, x_hash_t **r)
{
	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}
	
	*r = xmmsc_deserialize_hashtable (res->reply);

	if (!*r) {
		return 0;
	}

	return 1;
}


/**
 * Retrives a #x_list_t containing strings from the resultset.
 * @param res a #xmmsc_result_t containing a stringlist.
 * @param r the return #x_list_t.
 * @ret 1 upon success otherwise 0
 */

int
xmmsc_result_get_stringlist (xmmsc_result_t *res, x_list_t **r)
{
	x_list_t *list = NULL;
	char *tmp;
	int len;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	while (42) {
		if (!xmms_ipc_msg_get_string_alloc (res->reply, &tmp, &len))
			break;
		list = x_list_prepend (list, tmp);
	}

	list = x_list_reverse (list);

	if (!list)
		*r = NULL;
	else
		*r = list;


	return 1;
}

/**
 * Retrives a #x_list_t containing unsigned integers from the resultset.
 * @param res a #xmmsc_result_t containing a uintlist.
 * @param r the return #x_list_t.
 * @ret 1 upon success otherwise 0
 */

int
xmmsc_result_get_uintlist (xmmsc_result_t *res, x_list_t **r)
{
	x_list_t *list = NULL;
	unsigned int tmp;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	while (42) {
		if (!xmms_ipc_msg_get_uint32 (res->reply, &tmp))
			break;
		list = x_list_prepend (list, XUINT_TO_POINTER (tmp));
	}

	list = x_list_reverse (list);

	if (!list)
		*r = NULL;
	else
		*r = list;


	return 1;
}

/**
 * Retrives a #x_list_t containing signed integers from the resultset.
 * @param res a #xmmsc_result_t containing a intlist.
 * @param r the return #x_list_t.
 * @ret 1 upon success otherwise 0
 */

int
xmmsc_result_get_intlist (xmmsc_result_t *res, x_list_t **r)
{
	x_list_t *list = NULL;
	int tmp;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	while (42) {
		if (!xmms_ipc_msg_get_int32 (res->reply, &tmp))
			break;
		list = x_list_prepend (list, XINT_TO_POINTER (tmp));
	}

	list = x_list_reverse (list);

	if (!list)
		*r = NULL;
	else
		*r = list;


	return 1;
}

/**
 * Retrives a #x_list_t containing hashes from the resultset.
 * @param res a #xmmsc_result_t containing a entrylist.
 * @param r the return #x_list_t.
 * @ret 1 upon success otherwise 0
 */

int
xmmsc_result_get_hashlist (xmmsc_result_t *res, x_list_t **r)
{
	x_list_t *list = NULL;
	x_hash_t *e = NULL;

	if (!res || res->error != XMMS_ERROR_NONE || !res->reply) {
		return 0;
	}

	while (42) {
		e = xmmsc_deserialize_hashtable (res->reply);
		if (e) 
			list = x_list_prepend (list, e);
		else
			break;
	}

	list = x_list_reverse (list);

	if (!list)
		*r = NULL;
	else
		*r = list;


	return 1;
}


/** @} */
