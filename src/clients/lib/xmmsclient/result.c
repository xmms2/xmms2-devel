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

static void xmmsc_result_cleanup_data (xmmsc_result_t *res);
static x_hash_t *xmmsc_deserialize_hashtable (xmms_ipc_msg_t *msg);

typedef struct xmmsc_result_value_St {
	union {
		void *generic;
		uint32_t uint32;
		int32_t int32;
		char *string;
		x_hash_t *dict;
	} value;
	xmmsc_result_value_type_t type;
} xmmsc_result_value_t;

static xmmsc_result_value_t *xmmsc_result_parse_value (xmms_ipc_msg_t *msg);

struct xmmsc_result_St {
	xmmsc_connection_t *c;

	/** refcounting */
	int ref;

	/** notifiers */
	x_list_t *func_list;
	x_list_t *udata_list;

	int error;
	char *error_str;

	int islist;

	uint32_t cid;
	uint32_t restart_signal;

	xmmsc_ipc_t *ipc;

	uint32_t datatype;

	int parsed;

	union {
		void *generic;
		uint32_t uint;
		int32_t inte;
		char *string;
		x_hash_t *hash;
	} data;

	x_list_t *list;
	x_list_t *current;
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

	if (res->error_str)
		free (res->error_str);

	/* Free memory! */

	xmmsc_result_cleanup_data (res);
	xmmsc_ipc_result_unregister (res->ipc, res);

	xmmsc_unref (res->c);

	x_list_free (res->func_list);
	x_list_free (res->udata_list);
	
	free (res);
}

/**
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
 *   res = xmmsc_signal_playback_playtime (connection);
 *   xmmsc_result_notifier_set (res, handler);
 *   xmmsc_result_unref (res);
 * }
 * @endcode
 * In the above example the #handler would be called when the playtime is updated.
 * Only signals are restatable. Broadcasts will automaticly restart.
 */
xmmsc_result_t *
xmmsc_result_restart (xmmsc_result_t *res)
{
	xmmsc_result_t *newres;
	xmms_ipc_msg_t *msg;
	x_list_t *n, *l;

	x_return_null_if_fail (res);
	x_return_null_if_fail (res->c);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);
	xmms_ipc_msg_put_uint32 (msg, res->restart_signal);
	
	newres = xmmsc_send_msg (res->c, msg);
	
	l = res->udata_list;
	for (n = res->func_list; n; n = x_list_next (n)) {
		xmmsc_result_notifier_set (newres, n->data, l->data);
		l->data = NULL;
		n->data = NULL;
		l = x_list_next (l);
	}
	xmmsc_result_restartable (newres, res->restart_signal);
	
	return newres;
}

/**
 * Get the type of the result.
 * @returns The data type in the result or -1 on error.
 */
int
xmmsc_result_get_type (xmmsc_result_t *res)
{
	if (!res) return -1;
	if (!res->parsed) return -1;
	return res->datatype;
}

static void
xmmsc_result_value_free (void *v)
{
	xmmsc_result_value_t *val = v;

	switch (val->type) {
		case XMMS_OBJECT_CMD_ARG_STRING:
			free (val->value.string);
			break;
		default:
			break;
	}

	free (val);
}

static void
xmmsc_result_cleanup_data (xmmsc_result_t *res)
{
	x_list_t *l;

	x_return_if_fail (res);
	if (!res->parsed)
		return;

	if (res->islist) {
		res->datatype = XMMS_OBJECT_CMD_ARG_LIST;
	}

	switch (res->datatype) {
		case XMMS_OBJECT_CMD_ARG_UINT32 :
		case XMMS_OBJECT_CMD_ARG_INT32 :
			break;
		case XMMS_OBJECT_CMD_ARG_STRING :
			free (res->data.string);
			break;
		case XMMS_OBJECT_CMD_ARG_LIST:
			for (l = res->list; l; l = x_list_next (l)) {
				xmmsc_result_value_free (l->data);
			}
			x_list_free (res->list);
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			x_hash_destroy (res->data.hash);
			break;
	}
}

static bool
xmmsc_result_parse_msg (xmmsc_result_t *res, xmms_ipc_msg_t *msg)
{
	int type;
	x_list_t *list = NULL;

	if (xmmsc_result_iserror (res)) {
		res->parsed = true;
		return true;
	}

	if (!xmms_ipc_msg_get_int32 (msg, &type))
		return false;

	res->datatype = type;

	switch (type) {

		case XMMS_OBJECT_CMD_ARG_UINT32 :
			if (!xmms_ipc_msg_get_uint32 (msg, &res->data.uint)) {
				return false;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_INT32 :
			if (!xmms_ipc_msg_get_int32 (msg, &res->data.inte)) {
				return false;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_STRING :
			{
				uint32_t len;

				if (!xmms_ipc_msg_get_string_alloc (msg, &res->data.string, &len))
					return false;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			{
				x_hash_t *hash;

				hash = xmmsc_deserialize_hashtable (msg);
				if (!hash)
					return false;

				res->data.hash = hash;

			}
			break;
		case XMMS_OBJECT_CMD_ARG_LIST :
			{
				uint32_t len, i;

				if (!xmms_ipc_msg_get_uint32 (msg, &len))
					return false;

				for (i = 0; i < len; i ++) {
					xmmsc_result_value_t *val;
					val = xmmsc_result_parse_value (msg);
					list = x_list_prepend (list, val);
				}

				if (list)
					list = x_list_reverse (list);

				res->current = res->list = list;
				res->islist = 1;

				if (res->current) {
					xmmsc_result_value_t *val = res->current->data;
					res->data.generic = val->value.generic;
					res->datatype = val->type;
				} else {
					res->data.generic = NULL;
					res->datatype = XMMS_OBJECT_CMD_ARG_NONE;
				}
			}
			break;

		case XMMS_OBJECT_CMD_ARG_NONE :
			break;

		default :
			return false;
	}

	res->parsed = true;

	return true;
}


/**
 * return the command id of a resultset.
 */
int
xmmsc_result_cid (xmmsc_result_t *res)
{
	if (!res) {
		return 0;
	}

	return res->cid;
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

	res->func_list = x_list_append (res->func_list, func);
	res->udata_list = x_list_append (res->udata_list, user_data);
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
		xmmsc_result_seterror (res, strdup (err));
	}
}
				

/**
 * @defgroup ResultValueRetrieval ResultValueRetrieval 
 * @ingroup Result
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
 * @return 1 if error was encountered, else 0
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
 * Retrives a signed integer from the resultset.
 * @param res a #xmmsc_result_t containing a integer.
 * @param r the return integer.
 * @return 1 upon success otherwise 0
 */

int
xmmsc_result_get_int (xmmsc_result_t *res, int *r)
{
	if (!res || res->error != XMMS_ERROR_NONE) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_INT32) {
		return 0;
	}

	*r = res->data.inte;
	
	return 1;
}

/**
 * Retrives a unsigned integer from the resultset.
 * @param res a #xmmsc_result_t containing a integer.
 * @param r the return integer.
 * @return 1 upon success otherwise 0
 */

int
xmmsc_result_get_uint (xmmsc_result_t *res, unsigned int *r)
{
	if (!res || res->error != XMMS_ERROR_NONE) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_UINT32)
		return 0;

	*r = res->data.uint;

	return 1;
}

/**
 * Retrives a string from the resultset.
 * @param res a #xmmsc_result_t containing a string.
 * @param r the return string. This string is owned by the result and will be freed when the result is freed.
 * @return 1 upon success otherwise 0
 */
int
xmmsc_result_get_string (xmmsc_result_t *res, char **r)
{
	if (!res || res->error != XMMS_ERROR_NONE) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_STRING) {
		return 0;
	}

	*r = res->data.string;

	return 1;
}

/**
 * Retrieve integer associated for specified key in the resultset.
 *
 * If the key doesn't exist in the result the returned integer is
 * undefined. 
 *
 * @param res a #xmmsc_result_t containing a hashtable.
 * @param r the return int
 * @return 1 upon success otherwise 0
 *
 */
int
xmmsc_result_get_dict_entry_int32 (xmmsc_result_t *res, const char *key, int32_t *r)
{
	xmmsc_result_value_t *val;
	if (!res || res->error != XMMS_ERROR_NONE) {
		*r = -1;
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT) {
		*r = -1;
		return 0;
	}

	val = x_hash_lookup (res->data.hash, key);
	if (val && val->type == XMMSC_RESULT_VALUE_TYPE_INT32) {
		*r = val->value.int32;
	} else {
		*r = -1;
		return 0;
	}
	
	return 1;
}

/**
 * Retrieve unsigned integer associated for specified key in the resultset.
 *
 * If the key doesn't exist in the result the returned integer is
 * undefined. 
 *
 * @param res a #xmmsc_result_t containing a hashtable.
 * @param r the return uint
 * @return 1 upon success otherwise 0
 *
 */
int
xmmsc_result_get_dict_entry_uint32 (xmmsc_result_t *res, const char *key, uint32_t *r)
{
	xmmsc_result_value_t *val;
	if (!res || res->error != XMMS_ERROR_NONE) {
		*r = -1;
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT) {
		*r = -1;
		return 0;
	}

	val = x_hash_lookup (res->data.hash, key);
	if (val && val->type == XMMSC_RESULT_VALUE_TYPE_UINT32) {
		*r = val->value.uint32;
	} else {
		*r = -1;
		return 0;
	}
	
	return 1;
}

/**
 * Retrieve string associated for specified key in the resultset.
 *
 * If the key doesn't exist in the result the returned string is
 * NULL. The string is owned by the result and will be freed when the
 * result is freed.
 *
 * @param res a #xmmsc_result_t containing a hashtable.
 * @param r the return string (owned by result)
 * @return 1 upon success otherwise 0
 *
 */
int
xmmsc_result_get_dict_entry_str (xmmsc_result_t *res, const char *key, char **r)
{
	xmmsc_result_value_t *val;
	if (!res || res->error != XMMS_ERROR_NONE) {
		*r = NULL;
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT) {
		*r = NULL;
		return 0;
	}

	val = x_hash_lookup (res->data.hash, key);
	if (val && val->type == XMMSC_RESULT_VALUE_TYPE_STRING) {
		*r = val->value.string;
	} else {
		*r = NULL;
		return 0;
	}
	
	return 1;
}

/**
 * Retrieve type associated for specified key in the resultset.
 *
 * @param res a #xmmsc_result_t containing a hashtable.
 * @param key Key that should be retrieved
 * @return type of #key
 *
 */
xmmsc_result_value_type_t
xmmsc_result_get_dict_entry_type (xmmsc_result_t *res, const char *key)
{
	xmmsc_result_value_t *val;
	if (!res || res->error != XMMS_ERROR_NONE) {
		return XMMSC_RESULT_VALUE_TYPE_NONE;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT) {
		return XMMSC_RESULT_VALUE_TYPE_NONE;
	}

	val = x_hash_lookup (res->data.hash, key);
	if (!val) {
		return XMMSC_RESULT_VALUE_TYPE_NONE;
	}
	
	return val->type;
}



typedef struct {
	xmmsc_foreach_func func;
	void *userdata;
} xmmsc_foreach_t;

static void
xmmsc_result_dict_foreach_cb (const void *key, const void *value, void *udata)
{
	xmmsc_foreach_t *foreach = udata;
	const xmmsc_result_value_t *val = value;

	foreach->func (key, val->type, (void *)val->value.string, foreach->userdata);
}


/**
 * Iterate over all key/value-pair in the resultset.
 *
 * Calls specified function for each key/value-pair in the dictionary.
 * 
 * void function (const void *key, #xmmsc_result_value_type_t type, const void *value, void *user_data);
 *
 * @param res a #xmmsc_result_t containing a dict.
 * @param func function that is called for each key/value-pair
 * @param user_data extra data passed to func
 * @return 1 upon success otherwise 0
 *
 */
int
xmmsc_result_dict_foreach (xmmsc_result_t *res, xmmsc_foreach_func func, void *user_data)
{
	xmmsc_foreach_t fc;

	if (!res || res->error != XMMS_ERROR_NONE) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT) {
		return 0;
	}

	memset(&fc, 0, sizeof(fc));
	fc.func = func;
	fc.userdata = user_data;

	x_hash_foreach (res->data.hash, xmmsc_result_dict_foreach_cb, &fc);

	return 1;
}

/**
 * Check if the result stores a list.
 *
 * @param res a #xmmsc_result_t
 * @return 1 if result stores a list, 0 otherwise.
 */
int
xmmsc_result_is_list (xmmsc_result_t *res)
{
	if (!res || res->error != XMMS_ERROR_NONE) {
		return 0;
	}

	return res->islist;
}

/**
 * Check if current listnode is inside list boundary.
 *
 * When xmmsc_result_list_valid returns 1, there is a list entry
 * available for access with xmmsc_result_get_{type}.
 *
 * @param res a #xmmsc_result_t that is a list.
 * @return 1 if inside, 0 otherwise
 */
int
xmmsc_result_list_valid (xmmsc_result_t *res)
{
	if (!res || res->error != XMMS_ERROR_NONE) {
		return 0;
	}

	if (!res->islist) {
		return 0;
	}

	return !!res->current;
}

/**
 * Skip to next entry in list.
 *
 * Advances to next list entry. May advance outside of list, so
 * #xmmsc_result_list_valid should be used to determine if end of list
 * was reached.
 *
 * @param res a #xmmsc_result_t that is a list.
 * @return 1 upon succes, 0 otherwise
 */
int
xmmsc_result_list_next (xmmsc_result_t *res)
{
	if (!res || res->error != XMMS_ERROR_NONE) {
		return 0;
	}

	if (!res->islist) {
		return 0;
	}

	if (!res->current) {
		return 0;
	}

	res->current = res->current->next;
	
	if (res->current) {
		xmmsc_result_value_t *val = res->current->data;
		res->data.generic = val->value.generic;
		res->datatype = val->type;
	} else {
		res->data.generic = NULL;
		res->datatype = XMMS_OBJECT_CMD_ARG_NONE;
	}

	return 1;
}

/**
 * Return to first entry in list.
 *
 * @param res a #xmmsc_result_t that is a list.
 * @return 1 upon succes, 0 otherwise
 */
int
xmmsc_result_list_first (xmmsc_result_t *res)
{
	if (!res || res->error != XMMS_ERROR_NONE) {
		return 0;
	}

	res->current = res->list;

	if (res->current) {
		res->data.generic = res->current->data;
	} else {
		res->data.generic = NULL;
	}

	return 1;
}


/** @} */

/** @} */

/** @internal */
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
	x_list_t *n, *l;
	int cmd;
	x_return_if_fail (res);
	x_return_if_fail (msg);

	if (!xmmsc_result_parse_msg (res, msg)) {
		xmms_ipc_msg_destroy (msg);
		return;
	}

	cmd = xmms_ipc_msg_get_cmd (msg);

	xmms_ipc_msg_destroy (msg);
	
	xmmsc_result_ref (res);

	if (res->func_list) {
		l = res->udata_list;
		for (n = res->func_list; n; n = x_list_next (n)) {
			xmmsc_result_notifier_t notifier = n->data;
			if (notifier) {
				notifier (res, l->data);
			}
			l = x_list_next (l);
		}
	}

	if (cmd == XMMS_IPC_CMD_BROADCAST) {
		/* Post-cleanup of broadcast callback
		 * User have to make sure that he DOESN'T
		 * save the resultset somewhere! */
		xmmsc_result_cleanup_data (res);
	}

	xmmsc_result_unref (res);
}

/**
 * Allocates new #xmmsc_result_t and refereces it.
 * Should not be used from a client.
 * @internal
 */

xmmsc_result_t *
xmmsc_result_new (xmmsc_connection_t *c, uint32_t commandid)
{
	xmmsc_result_t *res;

	if (!(res = x_new0 (xmmsc_result_t, 1))) {
		x_oom();
		return NULL;
	}

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

static xmmsc_result_value_t *
xmmsc_result_parse_value (xmms_ipc_msg_t *msg)
{
	xmmsc_result_value_t *val;
	uint32_t len;

	if (!(val = x_new0 (xmmsc_result_value_t, 1))) {
		x_oom();
		return NULL;
	}

	if (!xmms_ipc_msg_get_int32 (msg, (int32_t *)&val->type)) {
		goto err;
	}

	switch (val->type) {
		case XMMS_OBJECT_CMD_ARG_STRING:
			if (!xmms_ipc_msg_get_string_alloc (msg, &val->value.string, &len)) {
				goto err;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_UINT32:
			if (!xmms_ipc_msg_get_uint32 (msg, &val->value.uint32)) {
				goto err;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_INT32:
			if (!xmms_ipc_msg_get_int32 (msg, &val->value.int32)) {
				goto err;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			val->value.dict = xmmsc_deserialize_hashtable (msg);
			if (!val->value.dict) {
				goto err;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_NONE:
			break;
		default:
			goto err;
			break;
	}

	return val;

err:
	x_internal_error ("Message from server did not parse correctly!");
	free (val);
	return NULL;

}

static x_hash_t *
xmmsc_deserialize_hashtable (xmms_ipc_msg_t *msg)
{
	unsigned int entries;
	unsigned int i;
	unsigned int len;
	x_hash_t *h;
	char *key;

	if (!xmms_ipc_msg_get_uint32 (msg, &entries)) {
		return NULL;
	}

	h = x_hash_new_full (x_str_hash, x_str_equal, free, xmmsc_result_value_free);

	for (i = 1; i <= entries; i++) {
		xmmsc_result_value_t *val;

		if (!xmms_ipc_msg_get_string_alloc (msg, &key, &len)) {
			goto err;
		}

		val = xmmsc_result_parse_value (msg);
		if (!val) {
			goto err;
		}

		x_hash_insert (h, key, val);
	}

	return h;

err:
	x_internal_error ("Message from server did not parse correctly!");
	x_hash_destroy (h);
	return NULL;
}
