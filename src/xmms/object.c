/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "xmms/xmms_object.h"
#include "xmms/xmms_log.h"
#include "xmmsc/xmmsc_idnumbers.h"

#include <stdarg.h>
#include <string.h>

/** @defgroup Object Object
  * @ingroup XMMSServer
  * @brief Object representation in XMMS server. A object can
  * be used to emit signals.
  * @{
  */

/**
 * A signal handler and it's data.
 */
typedef struct {
	xmms_object_handler_t handler;
	gpointer userdata;
} xmms_object_handler_entry_t;


/**
 * Cleanup all the resources for the object
 */
void
xmms_object_cleanup (xmms_object_t *object)
{
	gint i;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));

	for (i = 0; i < XMMS_IPC_SIGNAL_END; i++) {
		if (object->signals[i]) {
			GList *list = object->signals[i];

			while (list) {
				g_free (list->data);
				list = g_list_remove_link (list, list);
			}
		}
	}

	g_mutex_free (object->mutex);
}


/**
  * Connect to a signal that is emitted by this object.
  * You can connect many handlers to the same signal as long as
  * the handler address is unique.
  *
  * @todo fix the need for a unique handler adress?
  *
  * @param object the object that will emit the signal
  * @param signalid the signalid to connect to @sa signal_xmms.h
  * @param handler the Callback function to be called when signal is emited.
  * @param userdata data to the callback function
  */

void
xmms_object_connect (xmms_object_t *object, guint32 signalid,
                     xmms_object_handler_t handler, gpointer userdata)
{
	GList *list = NULL;
	GList *node;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (handler);

	entry = g_new0 (xmms_object_handler_entry_t, 1);
	entry->handler = handler;
	entry->userdata = userdata;

	if (object->signals[signalid]) {
		node = object->signals[signalid];
		list = g_list_prepend (node, entry);
		object->signals[signalid] = list;
	} else {
		list = g_list_prepend (list, entry);
		object->signals[signalid] = list;
	}

}

/**
  * Disconnect from a signal
  */

void
xmms_object_disconnect (xmms_object_t *object, guint32 signalid,
                        xmms_object_handler_t handler, gpointer userdata)
{
	GList *list = NULL, *node;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (handler);

	g_mutex_lock (object->mutex);

	list = object->signals[signalid];

	for (node = list; node; node = g_list_next (node)) {
		entry = node->data;

		if (entry->handler == handler && entry->userdata == userdata)
			break;
	}

	if (node)
		object->signals[signalid] = g_list_remove_link (list, node);
	g_mutex_unlock (object->mutex);

	g_return_if_fail (node);

	g_free (node->data);
	g_list_free_1 (node);
}

/**
  * Emit a signal and thus call all the handlers that are connected.
  *
  * @param object the object to signal on.
  * @param signalid the signalid to emit
  * @param data the data that should be sent to the handler.
  */

void
xmms_object_emit (xmms_object_t *object, guint32 signalid, gconstpointer data)
{
	GList *list, *node, *list2 = NULL;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));

	g_mutex_lock (object->mutex);

	list = object->signals[signalid];
	for (node = list; node; node = g_list_next (node)) {
		entry = node->data;

		list2 = g_list_prepend (list2, entry);
	}

	g_mutex_unlock (object->mutex);

	while (list2) {
		entry = list2->data;

		if (entry && entry->handler)
			entry->handler (object, data, entry->userdata);

		list2 = g_list_remove_link (list2, list2);
	}
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_bin_new (GString *bin)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->value.bin = bin;
	val->type = XMMS_OBJECT_CMD_ARG_BIN;
	return val;

}

xmms_object_cmd_value_t *
xmms_object_cmd_value_str_new (const gchar *string)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->value.string = g_strdup (string);
	val->type = XMMS_OBJECT_CMD_ARG_STRING;
	return val;
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_uint_new (guint32 uint)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->value.uint32 = uint;
	val->type = XMMS_OBJECT_CMD_ARG_UINT32;
	return val;
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_int_new (gint32 i)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->value.int32 = i;
	val->type = XMMS_OBJECT_CMD_ARG_INT32;
	return val;
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_dict_new (GHashTable *dict)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->value.dict = dict;
	val->type = XMMS_OBJECT_CMD_ARG_DICT;
	return val;
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_propdict_new (GList *list)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->value.list = list;
	val->type = XMMS_OBJECT_CMD_ARG_PROPDICT;
	return val;
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_list_new (GList *list)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->value.list = list;
	val->type = XMMS_OBJECT_CMD_ARG_LIST;
	return val;
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_coll_new (xmmsc_coll_t *coll)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->value.coll = coll;
	val->type = XMMS_OBJECT_CMD_ARG_COLL;
	return val;
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_none_new (void)
{
	xmms_object_cmd_value_t *val;
	val = g_new0 (xmms_object_cmd_value_t, 1);
	val->type = XMMS_OBJECT_CMD_ARG_NONE;
	return val;
}

xmms_object_cmd_value_t *
xmms_object_cmd_value_copy (xmms_object_cmd_value_t *val)
{
	xmms_object_cmd_value_t *ret = NULL;

	g_return_val_if_fail (val, NULL);

	switch (val->type) {
		case XMMS_OBJECT_CMD_ARG_BIN:
			ret = xmms_object_cmd_value_bin_new (val->value.bin);
			break;
		case XMMS_OBJECT_CMD_ARG_STRING:
			ret = xmms_object_cmd_value_str_new (val->value.string);
			break;
		case XMMS_OBJECT_CMD_ARG_UINT32:
			ret = xmms_object_cmd_value_uint_new (val->value.uint32);
			break;
		case XMMS_OBJECT_CMD_ARG_INT32:
			ret = xmms_object_cmd_value_int_new (val->value.int32);
			break;
		case XMMS_OBJECT_CMD_ARG_NONE:
			ret = xmms_object_cmd_value_none_new ();
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
		case XMMS_OBJECT_CMD_ARG_LIST:
		case XMMS_OBJECT_CMD_ARG_PROPDICT:
		case XMMS_OBJECT_CMD_ARG_STRINGLIST:
		case XMMS_OBJECT_CMD_ARG_COLL:
			/** Unsupported for now */
			XMMS_DBG ("Unsupported value passed to value_copy()");
			break;
	}

	return ret;
}

void
xmms_object_cmd_value_free (gpointer val)
{
	xmms_object_cmd_value_t *v = val;
	g_return_if_fail (v);

	switch (v->type) {
		case XMMS_OBJECT_CMD_ARG_STRING:
			if (v->value.string)
				g_free (v->value.string);
			break;
		case XMMS_OBJECT_CMD_ARG_BIN:
			if (v->value.bin)
				g_string_free (v->value.bin, TRUE);
			break;
		case XMMS_OBJECT_CMD_ARG_LIST:
		case XMMS_OBJECT_CMD_ARG_PROPDICT:
			while (v->value.list) {
				xmms_object_cmd_value_free (v->value.list->data);
				v->value.list = g_list_delete_link (v->value.list,
				                                    v->value.list);
			}
			break;
		case XMMS_OBJECT_CMD_ARG_STRINGLIST:
			while (v->value.list) {
				g_free (v->value.list->data);
				v->value.list = g_list_delete_link (v->value.list,
				                                    v->value.list);
			}
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			if (v->value.dict) {
				g_hash_table_destroy (v->value.dict);
			}

			break;
		case XMMS_OBJECT_CMD_ARG_COLL:
			if (v->value.coll) {
				xmmsc_coll_unref (v->value.coll);
			}
			break;
		default:
			break;
	}

	g_free (v);
}

/**
 * Initialize a command argument.
 */

void
xmms_object_cmd_arg_init (xmms_object_cmd_arg_t *arg)
{
	g_return_if_fail (arg);

	memset (arg, 0, sizeof (xmms_object_cmd_arg_t));
	xmms_error_reset (&arg->error);
}

/**
 * Emits a signal on the current object. This is like xmms_object_emit
 * but you don't have to create the #xmms_object_cmd_arg_t yourself.
 * Use this when you creating non-complex signal arguments.
 *
 * @param object Object to signal on.
 * @param signalid Signal to emit.
 * @param type the argument type to emit followed by the argument data.
 *
 */

void
xmms_object_emit_f (xmms_object_t *object, guint32 signalid,
                    xmms_object_cmd_arg_type_t type, ...)
{
	va_list ap;
	xmms_object_cmd_arg_t arg;

	xmms_object_cmd_arg_init (&arg);

	va_start (ap, type);

	switch (type) {
		case XMMS_OBJECT_CMD_ARG_UINT32:
			arg.retval = xmms_object_cmd_value_uint_new (va_arg (ap, guint32));
			break;
		case XMMS_OBJECT_CMD_ARG_INT32:
			arg.retval = xmms_object_cmd_value_int_new (va_arg (ap, gint32));
			break;
		case XMMS_OBJECT_CMD_ARG_STRING:
			arg.retval = xmms_object_cmd_value_str_new (va_arg (ap, gchar *));
			break;
		case XMMS_OBJECT_CMD_ARG_BIN:
			arg.retval = xmms_object_cmd_value_bin_new ((GString *) va_arg (ap, gpointer));
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			arg.retval = xmms_object_cmd_value_dict_new ((GHashTable *) va_arg (ap, gpointer));
			break;
		case XMMS_OBJECT_CMD_ARG_LIST:
		case XMMS_OBJECT_CMD_ARG_PROPDICT:
		case XMMS_OBJECT_CMD_ARG_STRINGLIST:
			arg.retval = xmms_object_cmd_value_list_new ((GList *) va_arg (ap, gpointer));
			break;
		case XMMS_OBJECT_CMD_ARG_COLL:
			arg.retval = xmms_object_cmd_value_coll_new ((xmmsc_coll_t *) va_arg (ap, gpointer));
			break;
		case XMMS_OBJECT_CMD_ARG_NONE:
			arg.retval = xmms_object_cmd_value_none_new ();
			break;
	}
	va_end (ap);

	xmms_object_emit (object, signalid, &arg);

	/*
	 * Let's not use value_free since that will free whatever
	 * is in the struct also. This should be owned by the
	 * parent
	 */
	if (type != XMMS_OBJECT_CMD_ARG_NONE)
		g_free (arg.retval);

}


/**
  * Add a command that could be called from the client API to a object.
  *
  * @param object The object that should have the method.
  * @param cmdid A command id.
  * @param desc A command description.
  */
void
xmms_object_cmd_add (xmms_object_t *object, guint cmdid,
                     xmms_object_cmd_desc_t *desc)
{
	g_return_if_fail (object);
	g_return_if_fail (desc);

	object->cmds[cmdid] = desc;
}

/**
  * Call a command with argument.
  */

void
xmms_object_cmd_call (xmms_object_t *object, guint cmdid, xmms_object_cmd_arg_t *arg)
{
	xmms_object_cmd_desc_t *desc;

	g_return_if_fail (object);

	desc = object->cmds[cmdid];

	if (desc->func)
		desc->func (object, arg);
}

/** @} */

void
__int_xmms_object_unref (xmms_object_t *object)
{
	g_return_if_fail (object->ref > 0);
	object->ref--;
	if (object->ref == 0) {
		xmms_object_emit (object, XMMS_IPC_SIGNAL_OBJECT_DESTROYED, NULL);
		if (object->destroy_func)
			object->destroy_func (object);
		xmms_object_cleanup (object);
		g_free (object);
	}
}

xmms_object_t *
__int_xmms_object_new (gint size, xmms_object_destroy_func_t destfunc)
{
	xmms_object_t *ret;

	ret = g_malloc0 (size);
	ret->destroy_func = destfunc;
	ret->id = XMMS_OBJECT_MID;

	ret->mutex = g_mutex_new ();
	xmms_object_ref (ret);

	return ret;
}

