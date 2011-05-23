/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

static xmmsv_t *xmms_create_xmmsv_list (GList *list);
static xmmsv_t *xmms_create_xmmsv_dict (GTree *dict);
static void create_xmmsv_list_foreach (gpointer data, gpointer userdata);
static gboolean create_xmmsv_dict_foreach (gpointer key, gpointer data, gpointer userdata);


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

static gboolean
cleanup_signal_list (gpointer key, gpointer value, gpointer data)
{
	GList *list = value;

	while (list) {
		g_free (list->data);
		list = g_list_delete_link (list, list);
	}

	return FALSE; /* keep going */
}

/**
 * Cleanup all the resources for the object
 */
void
xmms_object_cleanup (xmms_object_t *object)
{
	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));

	if (object->signals) {
		/* destroy the tree manually (ie not via a value_destroy_func
		 * callback since we're often "replacing" values when we're
		 * adding new elements to the signal lists. and we don't want
		 * the value to be destroyed in those cases :)
		 */
		g_tree_foreach (object->signals, cleanup_signal_list, NULL);
		g_tree_destroy (object->signals);
	}

	if (object->cmds) {
		/* We don't need to free the commands themselves -- they are
		 * stored in read-only memory.
		 */
		g_tree_destroy (object->cmds);
	}

	g_mutex_free (object->mutex);
}

static gint
compare_signal_key (gconstpointer a, gconstpointer b)
{
	gint aa = GPOINTER_TO_INT (a);
	gint bb = GPOINTER_TO_INT (b);

	if (aa < bb)
		return -1;
	else if (aa > bb)
		return 1;
	else
		return 0;
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
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (handler);

	entry = g_new0 (xmms_object_handler_entry_t, 1);
	entry->handler = handler;
	entry->userdata = userdata;

	if (!object->signals)
		object->signals = g_tree_new (compare_signal_key);
	else
		list = g_tree_lookup (object->signals,
		                      GINT_TO_POINTER (signalid));

	list = g_list_prepend (list, entry);

	/* store the list's new head in the tree */
	g_tree_insert (object->signals, GINT_TO_POINTER (signalid), list);
}

/**
  * Disconnect from a signal
  */

void
xmms_object_disconnect (xmms_object_t *object, guint32 signalid,
                        xmms_object_handler_t handler, gpointer userdata)
{
	GList *list, *node = NULL;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (handler);

	g_mutex_lock (object->mutex);

	if (object->signals) {
		list = g_tree_lookup (object->signals,
		                      GINT_TO_POINTER (signalid));

		for (node = list; node; node = g_list_next (node)) {
			entry = node->data;

			if (entry->handler == handler && entry->userdata == userdata)
				break;
		}

		if (node) {
			list = g_list_remove_link (list, node);

			/* store the list's new head in the tree */
			g_tree_insert (object->signals,
			               GINT_TO_POINTER (signalid), list);
		}
	}

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
xmms_object_emit (xmms_object_t *object, guint32 signalid, xmmsv_t *data)
{
	GList *list, *node, *list2 = NULL;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));

	g_mutex_lock (object->mutex);

	if (object->signals) {
		list = g_tree_lookup (object->signals,
		                      GINT_TO_POINTER (signalid));

		for (node = list; node; node = g_list_next (node)) {
			entry = node->data;

			list2 = g_list_prepend (list2, entry);
		}
	}

	g_mutex_unlock (object->mutex);

	while (list2) {
		entry = list2->data;

		/* NULL entries may never be added to the trees. */
		g_assert (entry);
		g_assert (entry->handler);

		entry->handler (object, data, entry->userdata);

		list2 = g_list_delete_link (list2, list2);
	}
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
                    xmmsv_type_t type, ...)
{
	va_list ap;
	xmmsv_t *arg;

	va_start (ap, type);

	switch (type) {
		case XMMSV_TYPE_NONE:
			arg = xmmsv_new_none ();
			break;
		case XMMSV_TYPE_INT32:
			arg = xmmsv_new_int (va_arg (ap, gint32));
			break;
		case XMMSV_TYPE_STRING:
			arg = xmmsv_new_string (va_arg (ap, gchar *));
			break;
		case XMMSV_TYPE_DICT:
			arg = xmms_create_xmmsv_dict (va_arg (ap, GTree *));
			break;
		case XMMSV_TYPE_END:
		default:
			XMMS_DBG ("OBJECT: trying to emit value of unsupported type (%d)!", (int)type);
			g_assert_not_reached ();
			break;
	}
	va_end (ap);

	xmms_object_emit (object, signalid, arg);

	/* In all cases above, we created a new xmmsv_t, which we
	 * now destroy.
	 * In some cases, those xmmsv_t's are created from GLib objects,
	 * such as GTrees. Here we must not destroy those GLib objects,
	 * because the caller wants to do that. However, the xmmsv_t's
	 * don't hold onto those GLib objects, so unreffing the
	 * xmmsv_t doesn't kill the GLib object.
	 */
	xmmsv_unref (arg);
}

static gint
compare_cmd_key (gconstpointer a, gconstpointer b)
{
	guint aa = GPOINTER_TO_INT (a);
	guint bb = GPOINTER_TO_INT (b);

	if (aa < bb)
		return -1;
	else if (aa > bb)
		return 1;
	else
		return 0;
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
                     const xmms_object_cmd_func_t func)
{
	g_return_if_fail (object);
	g_return_if_fail (func);

	if (!object->cmds)
		object->cmds = g_tree_new (compare_cmd_key);

	g_tree_insert (object->cmds, GUINT_TO_POINTER (cmdid),
	               (gpointer) func);
}

/**
  * Call a command with argument.
  */

void
xmms_object_cmd_call (xmms_object_t *object, guint cmdid, xmms_object_cmd_arg_t *arg)
{
	xmms_object_cmd_func_t func;

	g_return_if_fail (object);

	if (object->cmds) {
		func = g_tree_lookup (object->cmds, GUINT_TO_POINTER (cmdid));

		if (func)
			func (object, arg);
	}
}


/**
 * Create a new #xmmsv_t list initialized with the argument.
 * @param list The list of values to initially fill the #xmmsv_t with.
 * @return a new #xmmsv_t list.
 */
static xmmsv_t *
xmms_create_xmmsv_list (GList *list)
{
	xmmsv_t *v = xmmsv_new_list ();
	g_list_foreach (list, create_xmmsv_list_foreach, (gpointer) v);
	return v;
}

xmmsv_t *
xmms_convert_and_kill_list (GList *list)
{
	xmmsv_t *v;

	v = xmms_create_xmmsv_list (list);
	g_list_free (list);

	return v;
}

/**
 * Create a new #xmmsv_t dict initialized with the argument.
 * @param dict The dict of values to initially fill the #xmmsv_t with.
 * @return a new #xmmsv_t dict.
 */
static xmmsv_t *
xmms_create_xmmsv_dict (GTree *dict)
{
	xmmsv_t *v = NULL;
	if (dict) {
		v = xmmsv_new_dict ();
		g_tree_foreach (dict, create_xmmsv_dict_foreach, (gpointer) v);
	}
	return v;
}

xmmsv_t *
xmms_convert_and_kill_dict (GTree *dict)
{
	xmmsv_t *v;

	v = xmms_create_xmmsv_dict (dict);

	if (dict) {
		g_tree_destroy (dict);
	}

	return v;
}

xmmsv_t *
xmms_convert_and_kill_string (gchar *str)
{
	xmmsv_t *v = NULL;

	if (str) {
		v = xmmsv_new_string (str);
		g_free (str);
	}

	return v;
}

/** @} */

static void
create_xmmsv_list_foreach (gpointer data, gpointer userdata)
{
	xmmsv_t *v = (xmmsv_t *) data;
	xmmsv_t *l = (xmmsv_t *) userdata;

	xmmsv_list_append (l, v);

	/* Transfer ownership of 'v' from the GList to the
	 * xmmsv list.
	 */
	xmmsv_unref (v);
}

static gboolean
create_xmmsv_dict_foreach (gpointer key, gpointer data, gpointer userdata)
{
	const char *k = (const char *) key;
	xmmsv_t *v = (xmmsv_t *) data;
	xmmsv_t *l = (xmmsv_t *) userdata;
	xmmsv_dict_set (l, k, v);
	return FALSE;
}

int
xmms_bin_to_gstring (xmmsv_t *value, GString **gs)
{
	const guchar *str;
	guint len;
	if (!xmmsv_get_bin (value, &str, &len)) {
		return 0;
	}
	*gs = g_string_new_len (str, len);
	return 1;
}

int
dummy_identity (xmmsv_t *value, xmmsv_t **arg)
{
	*arg = value;
	return 1;
}

/**
 * Checks that the list only contains string values.
 */
gboolean
check_string_list (xmmsv_t *list)
{
	xmmsv_t *valstr;
	xmmsv_list_iter_t *it;

	for (xmmsv_get_list_iter (list, &it);
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it)) {
		xmmsv_list_iter_entry (it, &valstr);
		if (xmmsv_get_type (valstr) != XMMSV_TYPE_STRING) {
			return FALSE;
		}
	}

	return TRUE;
}


void
__int_xmms_object_unref (xmms_object_t *object)
{
	g_return_if_fail (object->ref > 0);
	if (g_atomic_int_dec_and_test (&(object->ref))) {
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

	/* don't create the trees for the signals and the commands yet.
	 * instead we instantiate those when we need them the first
	 * time.
	 */

	xmms_object_ref (ret);

	return ret;
}

