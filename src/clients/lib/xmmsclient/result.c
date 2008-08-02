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
#include <assert.h>

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_errorcodes.h"
#include "xmmsc/xmmsc_stdint.h"
#include "xmmsc/xmmsc_strlist.h"
#include "xmmsc/xmmsc_stdbool.h"

static void xmmsc_result_cleanup_data (xmmsc_result_t *res);
static void free_dict_list (x_list_t *list);
static x_list_t *xmmsc_deserialize_dict (xmms_ipc_msg_t *msg);
static int source_match_pattern (const char *source, const char *pattern);

typedef struct xmmsc_result_value_bin_St {
	unsigned char *data;
	uint32_t len;
} xmmsc_result_value_bin_t;

typedef struct xmmsc_result_value_St {
	union {
		void *generic;
		uint32_t uint32;
		int32_t int32;
		char *string;
		x_list_t *dict;
		xmmsc_coll_t *coll;
		xmmsc_result_value_bin_t *bin;
	} value;
	xmmsc_result_value_type_t type;
} xmmsc_result_value_t;

static xmmsc_result_value_t *xmmsc_result_parse_value (xmms_ipc_msg_t *msg);

typedef struct xmmsc_result_notifier_full_St {
	/** the function that's called when the result is processed */
	xmmsc_result_notifier_t func;

	/** user data to pass to func (optional) */
	void *udata;

	/* function that frees udata (optional) */
	xmmsc_user_data_free_func_t udata_free_func;
} xmmsc_result_notifier_full_t;

struct xmmsc_result_St {
	xmmsc_connection_t *c;

	/** refcounting */
	int ref;

	xmmsc_result_type_t type;

	/** notifiers */
	x_list_t *notifier_list;

	bool error;
	char *error_str;

	bool islist;

	uint32_t cookie;
	uint32_t restart_signal;

	xmmsc_ipc_t *ipc;

	uint32_t datatype;

	bool parsed;

	union {
		void *generic;
		uint32_t uint;
		int32_t inte;
		char *string;
		x_list_t *dict;
		xmmsc_coll_t *coll;
		xmmsc_result_value_bin_t *bin;
	} data;

	x_list_t *list;
	x_list_t *current;

	/* the list of sources from most to least prefered.
	 * if this is NULL, then default_source_pref will be used instead.
	 */
	char **source_pref;

	/* things we want to free when the result is freed*/
	x_list_t *extra_free;

	xmmsc_visualization_t *visc;
};

static const char *default_source_pref[] = {
	"server",
	"client/*",
	"plugin/id3v2",
	"plugin/segment",
	"plugin/*",
	"*",
	NULL
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
 * uint32_t id;
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
 *   uint32_t id;
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
 * When the answer arrives handler will be called. with the resulting #xmmsc_result_t
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

	if (res->error_str)
		free (res->error_str);

	/* Free memory! */

	xmmsc_result_cleanup_data (res);
	xmmsc_ipc_result_unregister (res->ipc, res);

	xmmsc_unref (res->c);

	while (res->notifier_list) {
		xmmsc_result_notifier_full_t *notifier = res->notifier_list->data;

		/* free the user data if set */
		if (notifier->udata_free_func && notifier->udata) {
			notifier->udata_free_func (notifier->udata);
		}

		free (notifier);

		res->notifier_list = x_list_delete_link (res->notifier_list,
		                                         res->notifier_list);
	}

	if (res->source_pref) {
		xmms_strlist_destroy (res->source_pref);
	}

	while (res->extra_free) {
		free (res->extra_free->data);
		res->extra_free = x_list_delete_link (res->extra_free,
		                                      res->extra_free);
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
 * Check the #xmmsc_result_t for error.
 * @return 1 if error was encountered, else 0
 */

int
xmmsc_result_iserror (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, 1);

	return !!res->error;
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

	x_return_null_if_fail (res);
	x_return_null_if_fail (res->c);

	x_api_error_if (res->type != XMMSC_RESULT_CLASS_SIGNAL,
	                "result is not restartable", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);
	xmms_ipc_msg_put_uint32 (msg, res->restart_signal);

	newres = xmmsc_send_msg (res->c, msg);

	/* The result's notifiers are moved over to newres. */
	if (newres->notifier_list) {
		x_internal_error ("restart result's notifiers non-empty!");
	}

	newres->notifier_list = res->notifier_list;
	res->notifier_list = NULL;

	/* Each pending call takes one ref */
	newres->ref += x_list_length (newres->notifier_list);

	xmmsc_result_restartable (newres, res->restart_signal);

	return newres;
}

static void
xmmsc_result_value_free (void *v)
{
	xmmsc_result_value_t *val = v;

	switch (val->type) {
		case XMMS_OBJECT_CMD_ARG_BIN:
			free (val->value.bin->data);
			free (val->value.bin);
			break;
		case XMMS_OBJECT_CMD_ARG_STRING:
			free (val->value.string);
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			free_dict_list (val->value.dict);
			break;
		case XMMS_OBJECT_CMD_ARG_COLL:
			xmmsc_coll_unref (val->value.coll);
			break;
		default:
			break;
	}

	free (val);
}

static void
xmmsc_result_cleanup_data (xmmsc_result_t *res)
{
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
			res->data.string = NULL;
			break;
		case XMMS_OBJECT_CMD_ARG_BIN :
			free (res->data.bin->data);
			free (res->data.bin);
			res->data.bin = NULL;
			break;
		case XMMS_OBJECT_CMD_ARG_LIST:
		case XMMS_OBJECT_CMD_ARG_PROPDICT:
			while (res->list) {
				xmmsc_result_value_free (res->list->data);
				res->list = x_list_delete_link (res->list, res->list);
			}

			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			free_dict_list (res->data.dict);
			res->data.dict = NULL;
			break;
		case XMMS_OBJECT_CMD_ARG_COLL:
			xmmsc_coll_unref (res->data.coll);
			res->data.coll = NULL;
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
		case XMMS_OBJECT_CMD_ARG_BIN:
			{
				xmmsc_result_value_bin_t *bin;
				bin = x_new0 (xmmsc_result_value_bin_t, 1);
				if (!xmms_ipc_msg_get_bin_alloc (msg, &bin->data, &bin->len)) {
					free (bin);
					return false;
				}
				res->data.bin = bin;
				break;
			}
		case XMMS_OBJECT_CMD_ARG_STRING :
			{
				uint32_t len;

				if (!xmms_ipc_msg_get_string_alloc (msg, &res->data.string, &len))
					return false;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			{
				x_list_t *dict;

				dict = xmmsc_deserialize_dict (msg);
				if (!dict)
					return false;

				res->data.dict = dict;

			}
			break;
		case XMMS_OBJECT_CMD_ARG_LIST :
		case XMMS_OBJECT_CMD_ARG_PROPDICT :
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

				if (type == XMMS_OBJECT_CMD_ARG_LIST) {
					res->islist = true;

					if (res->current) {
						xmmsc_result_value_t *val = res->current->data;
						res->data.generic = val->value.generic;
						res->datatype = val->type;
					} else {
						res->data.generic = NULL;
						res->datatype = XMMS_OBJECT_CMD_ARG_NONE;
					}
				}
			}
			break;

		case XMMS_OBJECT_CMD_ARG_COLL:
			{
				xmmsc_coll_t *coll;

				if (!xmms_ipc_msg_get_collection_alloc (msg, &coll))
					return false;

				res->data.coll = coll;
				xmmsc_coll_ref (res->data.coll);
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
 * will be called when the answer arrives. This function differs from xmmsc_result_notifier_set in the additional free_func parameter, which allows to pass a pointer to a function which will be called to free the user_data when needed.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param func the function that should be called when we receive the answer
 * @param user_data optional user data to the callback
 * @param free_func optional function that should be called to free the user_data
 */

void
xmmsc_result_notifier_set_full (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data, xmmsc_user_data_free_func_t free_func)
{
	xmmsc_result_notifier_full_t *notifier;

	x_return_if_fail (res);
	x_return_if_fail (func);

	notifier = x_new (xmmsc_result_notifier_full_t, 1);

	if (!notifier) {
		x_oom ();
		return;
	}

	notifier->func = func;
	notifier->udata = user_data;
	notifier->udata_free_func = free_func;

	res->notifier_list = x_list_append (res->notifier_list, notifier);

	/* The pending call takes one ref */
	xmmsc_result_ref (res);
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
		xmmsc_result_seterror (res, err);
	}
}

/**
 * Set sources to be used when fetching stuff from a propdict.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @param preference a list of sources from most to least preferrable.
 * You may use a wildcard "*" character.
 */
void
xmmsc_result_source_preference_set (xmmsc_result_t *res, const char **preference)
{
	x_return_if_fail (res);
	x_return_if_fail (preference);

	if (res->source_pref) {
		xmms_strlist_destroy (res->source_pref);
	}

	res->source_pref = xmms_strlist_copy ((char **) preference);
}

/**
 * Get sources to be used when fetching stuff from a propdict.
 * @param res a #xmmsc_result_t that you got from a command dispatcher.
 * @returns The current sources from most to least preferable, as a
 * NULL-terminated array of immutable strings.
 * This array is owned by the result and will be freed with it.
 */
const char **
xmmsc_result_source_preference_get (xmmsc_result_t *res)
{
	x_return_val_if_fail (res, NULL);

	if (res->source_pref)
		return (const char **) res->source_pref;
	else
		return default_source_pref;
}

/**
 * @defgroup ResultValueRetrieval ResultValueRetrieval
 * @ingroup Result
 * @brief Explains how you can retrive values from a #xmmsc_result_t
 * @{
 */

/**
 * Get the type of the result.
 * @returns The data type in the result.
 */
xmmsc_result_value_type_t
xmmsc_result_get_type (xmmsc_result_t *res)
{
	x_api_error_if (!res, "NULL result",
	                XMMSC_RESULT_VALUE_TYPE_NONE);
	x_api_error_if (!res->parsed, "invalid result type",
	                XMMSC_RESULT_VALUE_TYPE_NONE);
	return res->datatype;
}

/**
 * Retrives a signed integer from the resultset.
 * @param res a #xmmsc_result_t containing a integer.
 * @param r the return integer.
 * @return 1 upon success otherwise 0
 */

int
xmmsc_result_get_int (xmmsc_result_t *res, int32_t *r)
{
	if (xmmsc_result_iserror (res)) {
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
xmmsc_result_get_uint (xmmsc_result_t *res, uint32_t *r)
{
	if (xmmsc_result_iserror (res)) {
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
xmmsc_result_get_string (xmmsc_result_t *res, const char **r)
{
	if (xmmsc_result_iserror (res)) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_STRING) {
		return 0;
	}

	*r = res->data.string;

	return 1;
}

/**
 * Retrieves a collection from the resultset.
 * @param res a #xmmsc_result_t containing a collection.
 * @param c the return collection. This collection is owned by the result and will be freed when the result is freed.
 * @return 1 upon success otherwise 0
 */
int
xmmsc_result_get_collection (xmmsc_result_t *res, xmmsc_coll_t **c)
{
	if (xmmsc_result_iserror (res)) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_COLL) {
		return 0;
	}

	*c = res->data.coll;

	return 1;
}

/**
 * Retrives binary data from the resultset.
 * @param res a #xmmsc_result_t containing a string.
 * @param r the return data. This data is owned by the result and will be freed when the result is freed.
 * @param rlen the return length of data.
 * @return 1 upon success otherwise 0
 */
int
xmmsc_result_get_bin (xmmsc_result_t *res, unsigned char **r, unsigned int *rlen)
{
	if (xmmsc_result_iserror (res)) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_BIN) {
		return 0;
	}

	*r = res->data.bin->data;
	*rlen = res->data.bin->len;

	return 1;
}

static xmmsc_result_value_t *
plaindict_lookup (xmmsc_result_t *res, const char *key)
{
	x_list_t *n;

	for (n = res->data.dict; n; n = x_list_next (n)) {
		const char *k = n->data;
		if (strcasecmp (k, key) == 0 && n->next) {
			/* found right key, return value */
			return (xmmsc_result_value_t*) n->next->data;
		} else {
			/* skip data part of this entry */
			n = x_list_next (n);
		}
	}

	return NULL;
}

static xmmsc_result_value_t *
propdict_lookup (xmmsc_result_t *res, const char *key)
{
	x_list_t *n;
	const char **sources, **ptr;

	sources = res->source_pref ?
		(const char **) res->source_pref : default_source_pref;

	for (ptr = sources; *ptr; ptr++) {
		const char *source = *ptr;

		for (n = res->list; n; n = x_list_next (n)) {
			xmmsc_result_value_t *k = n->data;

			if (source_match_pattern (k->value.string, source) &&
			    n->next && n->next->next) {

				n = x_list_next (n);
				k = n->data;

				if (strcasecmp (k->value.string, key) == 0) {
					return (xmmsc_result_value_t*) n->next->data;
				} else {
					n = x_list_next (n);
				}

			} else {
				n = x_list_next (n);
				n = x_list_next (n);
			}
		}
	}

	return NULL;
}

static xmmsc_result_value_t *
xmmsc_result_dict_lookup (xmmsc_result_t *res, const char *key)
{
	if (res->datatype == XMMS_OBJECT_CMD_ARG_DICT) {
		return plaindict_lookup (res, key);
	} else if (res->datatype == XMMS_OBJECT_CMD_ARG_PROPDICT) {
		return propdict_lookup (res, key);
	}

	return NULL;
}

/**
 * Retrieve integer associated for specified key in the resultset.
 *
 * If the key doesn't exist in the result the returned integer is
 * undefined.
 *
 * @param res a #xmmsc_result_t containing dict list.
 * @param key Key that should be retrieved
 * @param r the return int
 * @return 1 upon success otherwise 0
 *
 */
int
xmmsc_result_get_dict_entry_int (xmmsc_result_t *res, const char *key, int32_t *r)
{
	xmmsc_result_value_t *val;
	if (xmmsc_result_iserror (res)) {
		*r = -1;
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT &&
	    res->datatype != XMMS_OBJECT_CMD_ARG_PROPDICT) {
		*r = -1;
		return 0;
	}

	val = xmmsc_result_dict_lookup (res, key);
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
 * @param key Key that should be retrieved
 * @param r the return uint
 * @return 1 upon success otherwise 0
 *
 */
int
xmmsc_result_get_dict_entry_uint (xmmsc_result_t *res, const char *key,
                                  uint32_t *r)
{
	xmmsc_result_value_t *val;
	if (xmmsc_result_iserror (res)) {
		*r = -1;
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT &&
	    res->datatype != XMMS_OBJECT_CMD_ARG_PROPDICT) {
		*r = -1;
		return 0;
	}

	val = xmmsc_result_dict_lookup (res, key);
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
 * @param key Key that should be retrieved
 * @param r the return string (owned by result)
 * @return 1 upon success otherwise 0
 *
 */
int
xmmsc_result_get_dict_entry_string (xmmsc_result_t *res,
                                    const char *key, const char **r)
{
	xmmsc_result_value_t *val;
	if (xmmsc_result_iserror (res)) {
		*r = NULL;
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT &&
	    res->datatype != XMMS_OBJECT_CMD_ARG_PROPDICT) {
		*r = NULL;
		return 0;
	}

	val = xmmsc_result_dict_lookup (res, key);
	if (val && val->type == XMMSC_RESULT_VALUE_TYPE_STRING) {
		*r = val->value.string;
	} else {
		*r = NULL;
		return 0;
	}

	return 1;
}

/**
 * Retrieve collection associated for specified key in the resultset.
 *
 * If the key doesn't exist in the result the returned collection is
 * NULL. The collection is owned by the result and will be freed when the
 * result is freed.
 *
 * @param res a #xmmsc_result_t containing a hashtable.
 * @param key Key that should be retrieved
 * @param c the return collection (owned by result)
 * @return 1 upon success otherwise 0
 *
 */
int
xmmsc_result_get_dict_entry_collection (xmmsc_result_t *res, const char *key,
                                        xmmsc_coll_t **c)
{
	xmmsc_result_value_t *val;
	if (xmmsc_result_iserror (res)) {
		*c = NULL;
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT &&
	    res->datatype != XMMS_OBJECT_CMD_ARG_PROPDICT) {
		*c = NULL;
		return 0;
	}

	val = xmmsc_result_dict_lookup (res, key);
	if (val && val->type == XMMSC_RESULT_VALUE_TYPE_COLL) {
		*c = val->value.coll;
	} else {
		*c = NULL;
		return 0;
	}

	return 1;
}

/**
 * Retrieve type associated for specified key in the resultset.
 *
 * @param res a #xmmsc_result_t containing a hashtable.
 * @param key Key that should be retrieved
 * @return type of key
 *
 */
xmmsc_result_value_type_t
xmmsc_result_get_dict_entry_type (xmmsc_result_t *res, const char *key)
{
	xmmsc_result_value_t *val;
	if (xmmsc_result_iserror (res)) {
		return XMMSC_RESULT_VALUE_TYPE_NONE;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT &&
	    res->datatype != XMMS_OBJECT_CMD_ARG_PROPDICT) {
		return XMMSC_RESULT_VALUE_TYPE_NONE;
	}

	val = xmmsc_result_dict_lookup (res, key);
	if (!val) {
		return XMMSC_RESULT_VALUE_TYPE_NONE;
	}

	return val->type;
}

int
xmmsc_result_propdict_foreach (xmmsc_result_t *res,
                               xmmsc_propdict_foreach_func func,
                               void *user_data)
{
	x_list_t *n;

	if (xmmsc_result_iserror (res)) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_PROPDICT) {
		x_print_err ("xmms_result_propdict_foreach", "on a normal dict!");
		return 0;
	}

	for (n = res->list; n; n = x_list_next (n)) {
		xmmsc_result_value_t *source = NULL;
		xmmsc_result_value_t *key = NULL;
		xmmsc_result_value_t *val = NULL;
		if (n->next && n->next->next) {
			source = n->data;
			key = n->next->data;
			val = n->next->next->data;
		}
		func ((const void *)key->value.string, val->type, (void *)val->value.string, source->value.string, user_data);
		n = x_list_next (n); /* skip key part */
		n = x_list_next (n); /* skip value part */
	}

	return 1;
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
xmmsc_result_dict_foreach (xmmsc_result_t *res, xmmsc_dict_foreach_func func,
                           void *user_data)
{
	x_list_t *n;

	if (xmmsc_result_iserror (res)) {
		return 0;
	}

	if (res->datatype != XMMS_OBJECT_CMD_ARG_DICT) {
		x_print_err ("xmms_result_dict_foreach", "on a source dict!");
		return 0;
	}

	if (res->datatype == XMMS_OBJECT_CMD_ARG_DICT) {
		for (n = res->data.dict; n; n = x_list_next (n)) {
			xmmsc_result_value_t *val = NULL;
			if (n->next) {
				val = n->next->data;
			}
			func ((const void *)n->data, val->type, (void *)val->value.string, user_data);
			n = x_list_next (n); /* skip value part */
		}
	}

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
	if (xmmsc_result_iserror (res)) {
		return 0;
	}

	return !!res->islist;
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
	if (xmmsc_result_iserror (res)) {
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
	if (xmmsc_result_iserror (res)) {
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
	if (xmmsc_result_iserror (res)) {
		return 0;
	}

	res->current = res->list;

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
 * Decode an URL-encoded string.
 *
 * Some strings (currently only the url of media) has no known
 * encoding, and must be encoded in an UTF-8 clean way. This is done
 * similar to the url encoding web browsers do. This functions decodes
 * a string encoded in that way. OBSERVE that the decoded string HAS
 * NO KNOWN ENCODING and you cannot display it on screen in a 100%
 * guaranteed correct way (a good heuristic is to try to validate the
 * decoded string as UTF-8, and if it validates assume that it is an
 * UTF-8 encoded string, and otherwise fall back to some other
 * encoding).
 *
 * Do not use this function if you don't understand the
 * implications. The best thing is not to try to display the url at
 * all.
 *
 * Note that the fact that the string has NO KNOWN ENCODING and CAN
 * NOT BE DISPLAYED does not stop you from open the file if it is a
 * local file (if it starts with "file://").
 *
 * The string returned string will be owned by the result and
 * freed when the result is freed. Or, if the result passed is NULL,
 * the user is responsible for freeing the returned string. However,
 * the user has no way of knowing what allocation routine was used to
 * create the string and thus no way to know which free routine to
 * use. Passing a NULL result is generall frowned upon and we won't
 * offer you tissues and a blanket if you come crying to us with
 * broken code.
 *
 * @param res the #xmmsc_result_t that the string comes from
 * @param string the url encoded string
 * @return decoded string, owned by the #xmmsc_result_t
 *
 */
const char *
xmmsc_result_decode_url (xmmsc_result_t *res, const char *string)
{
	int i = 0, j = 0;
	char *url;

	url = strdup (string);
	if (!url) {
		x_oom ();
		return NULL;
	}

	while (url[i]) {
		unsigned char chr = url[i++];

		if (chr == '+') {
			chr = ' ';
		} else if (chr == '%') {
			char ts[3];
			char *t;

			ts[0] = url[i++];
			if (!ts[0])
				goto err;
			ts[1] = url[i++];
			if (!ts[1])
				goto err;
			ts[2] = '\0';

			chr = strtoul (ts, &t, 16);

			if (t != &ts[2])
				goto err;
		}

		url[j++] = chr;
	}

	url[j] = '\0';

	if (res)
		res->extra_free = x_list_prepend (res->extra_free, url);

	return url;

 err:
	free (url);
	return NULL;
}

/** @} */

/** @} */

/** @internal */
void
xmmsc_result_seterror (xmmsc_result_t *res, const char *errstr)
{
	res->error_str = strdup (errstr);
	res->error = true;

	if (!res->error_str) {
		x_oom ();
	}
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
	x_list_t *n;
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

	for (n = res->notifier_list; n; n = x_list_next (n)) {
		xmmsc_result_notifier_full_t *notifier = n->data;

		if (notifier->func) {
			notifier->func (res, notifier->udata);
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
xmmsc_result_new (xmmsc_connection_t *c, xmmsc_result_type_t type,
                  uint32_t cookie)
{
	xmmsc_result_t *res;

	if (!(res = x_new0 (xmmsc_result_t, 1))) {
		x_oom ();
		return NULL;
	}

	res->c = xmmsc_ref (c);

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

static xmmsc_result_value_t *
xmmsc_result_parse_value (xmms_ipc_msg_t *msg)
{
	xmmsc_result_value_t *val;
	uint32_t len;

	if (!(val = x_new0 (xmmsc_result_value_t, 1))) {
		x_oom ();
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
			val->value.dict = xmmsc_deserialize_dict (msg);
			if (!val->value.dict) {
				goto err;
			}
			break;
		case XMMS_OBJECT_CMD_ARG_COLL:
			xmms_ipc_msg_get_collection_alloc (msg, &val->value.coll);
			if (!val->value.coll) {
				goto err;
			}
			xmmsc_coll_ref (val->value.coll);
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

static x_list_t *
xmmsc_deserialize_dict (xmms_ipc_msg_t *msg)
{
	unsigned int entries;
	unsigned int len;
	x_list_t *n = NULL;
	char *key;

	if (!xmms_ipc_msg_get_uint32 (msg, &entries)) {
		return NULL;
	}

	while (entries--) {
		xmmsc_result_value_t *val;

		if (!xmms_ipc_msg_get_string_alloc (msg, &key, &len)) {
			goto err;
		}

		val = xmmsc_result_parse_value (msg);
		if (!val) {
			free (key);
			goto err;
		}

		n = x_list_prepend (n, key);
		n = x_list_prepend (n, val);
	}

	return x_list_reverse (n);

err:
	x_internal_error ("Message from server did not parse correctly!");

	free_dict_list (x_list_reverse (n));

	return NULL;
}

static void
free_dict_list (x_list_t *list)
{
	while (list) {
		free (list->data); /* key */
		list = x_list_delete_link (list, list);

		/* xmmsc_deserialize_dict guarantees that the list is
		 * well-formed
		 */
		assert (list);

		xmmsc_result_value_free (list->data); /* value */
		list = x_list_delete_link (list, list);
	}
}

static int
source_match_pattern (const char *source, const char *pattern)
{
	int match = 0;
	int lpos = strlen (pattern) - 1;

	if (strcasecmp (pattern, source) == 0) {
		match = 1;
	}
	else if (lpos >= 0 && pattern[lpos] == '*' &&
	        (lpos == 0 || strncasecmp (source, pattern, lpos) == 0)) {
		match = 1;
	}

	return match;
}
