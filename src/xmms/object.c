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




#include "xmms/object.h"
#include "xmms/signal_xmms.h"
#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/playlist_entry.h"

#include <stdarg.h>

/** @defgroup Object Object
  * @ingroup XMMSServer
  * @{
  */

typedef struct {
	xmms_object_handler_t handler;
	gpointer userdata;
} xmms_object_handler_entry_t;

void
xmms_object_parent_set (xmms_object_t *object, xmms_object_t *parent)
{
	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));

	XMMS_MTX_LOCK (object->mutex);
	object->parent = parent;
	XMMS_MTX_UNLOCK (object->mutex);
}

static gboolean
xmms_object_cleanup_foreach (gpointer key, gpointer value, gpointer data)
{
	GList *list = value, *node;

	for (node = list; node; node = g_list_next (node)) {
		if (node->data)
			g_free (node->data);
	}
	if (list)
		g_list_free (list);

	if (key)
		g_free (key);

	return TRUE;
}
				 
void
xmms_object_cleanup (xmms_object_t *object)
{
	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));

	g_hash_table_foreach_remove (object->signals, xmms_object_cleanup_foreach, NULL);
	g_hash_table_destroy (object->signals);
	g_mutex_free (object->mutex);
}

void
xmms_object_connect (xmms_object_t *object, const gchar *signal,
					 xmms_object_handler_t handler, gpointer userdata)
{
	GList *list = NULL;
	gpointer key;
	gpointer val;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (signal);
	g_return_if_fail (handler);

	
	entry = g_new0 (xmms_object_handler_entry_t, 1);
	entry->handler = handler;
	entry->userdata = userdata;

/*	XMMS_MTX_LOCK (object->mutex);*/
	
	if (g_hash_table_lookup_extended (object->signals, signal, &key, &val)) {
		list = g_list_prepend (val, entry);
		g_hash_table_insert (object->signals, key, list);
	} else {
		list = g_list_prepend (list, entry);
		g_hash_table_insert (object->signals, g_strdup (signal), list);
	}

/*	XMMS_MTX_UNLOCK (object->mutex);*/
}

void
xmms_object_disconnect (xmms_object_t *object, const gchar *signal,
						xmms_object_handler_t handler)
{
	GList *list = NULL, *node;
	gpointer key;
	gpointer val;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (signal);
	g_return_if_fail (handler);

	XMMS_MTX_LOCK (object->mutex);
	
	if (!g_hash_table_lookup_extended (object->signals, signal, &key, &val))
		goto unlock;
	if (!val)
		goto unlock;

	list = (GList *)val;

	for (node = list; node; node = g_list_next (node)) {
		entry = node->data;

		if (entry->handler == handler)
			break;
	}

	if (!node)
		goto unlock;
	
	list = g_list_delete_link (list, node);
	if (!list) {
		g_hash_table_remove (object->signals, key);
		g_free (key);
	} else {
		g_hash_table_insert (object->signals, key, list);
	}
unlock:
	XMMS_MTX_UNLOCK (object->mutex);
}

void
xmms_object_emit (xmms_object_t *object, const gchar *signal, gconstpointer data)
{
	GList *list, *node, *list2 = NULL;
	xmms_object_handler_entry_t *entry;
	
	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (signal);

	XMMS_MTX_LOCK (object->mutex);
	
	list = g_hash_table_lookup (object->signals, signal);
	for (node = list; node; node = g_list_next (node)) {
		entry = node->data;

		list2 = g_list_prepend (list2, entry);
	}

	if (!list2) { /* no handler -> send to parent */
		if (object->parent) {
			xmms_object_emit (object->parent, signal, data);
		}
	}

	XMMS_MTX_UNLOCK (object->mutex);

	for (node = list2; node; node = g_list_next (node)) {
		entry = node->data;
		
		if (entry && entry->handler)
			entry->handler (object, data, entry->userdata);
	}
	g_list_free (list2);

}

void
xmms_object_emit_f (xmms_object_t *object, const gchar *signal,
		    xmms_object_method_arg_type_t type, ...)
{
	va_list ap;
	xmms_object_method_arg_t arg;

	va_start(ap, type);

	switch (type) {
		case XMMS_OBJECT_METHOD_ARG_UINT32:
			arg.retval.uint32 = va_arg (ap, guint32);
			break;
		case XMMS_OBJECT_METHOD_ARG_INT32:
			arg.retval.int32 = va_arg (ap, gint32);
			break;
		case XMMS_OBJECT_METHOD_ARG_STRING:
			arg.retval.string = va_arg (ap, gchar *);
			break;
		case XMMS_OBJECT_METHOD_ARG_PLAYLIST_ENTRY:
			arg.retval.playlist_entry = (xmms_playlist_entry_t *) va_arg (ap, gpointer);
			break;
		case XMMS_OBJECT_METHOD_ARG_UINTLIST:
			arg.retval.uintlist = (GList*) va_arg (ap, gpointer);
			break;
		case XMMS_OBJECT_METHOD_ARG_INTLIST:
			arg.retval.intlist = (GList*) va_arg (ap, gpointer);
			break;
		case XMMS_OBJECT_METHOD_ARG_STRINGLIST:
			arg.retval.stringlist = (GList*) va_arg (ap, gpointer);
			break;
		case XMMS_OBJECT_METHOD_ARG_PLCH:
			arg.retval.plch = (xmms_playlist_changed_msg_t *) va_arg (ap, gpointer);
			break;
		case XMMS_OBJECT_METHOD_ARG_NONE:
			break;
	}
	arg.rettype = type;
	va_end(ap);

	xmms_object_emit (object, signal, &arg);

}


void
xmms_object_method_add (xmms_object_t *object, char *method, xmms_object_method_func_t func)
{

	g_hash_table_insert (object->methods, method, func);

}


void
xmms_object_method_call (xmms_object_t *object, const char *method, xmms_object_method_arg_t *arg)
{
	xmms_object_method_func_t func;

	func = g_hash_table_lookup (object->methods, method);

	if (func)
		func (object, arg);
}

/** @} */

void
__int_xmms_object_unref (xmms_object_t *object)
{
	object->ref--;
	if (object->ref == 0) {
		XMMS_DBG ("Free %p", object);
		xmms_object_emit (object, XMMS_SIGNAL_OBJECT_DESTROYED, NULL);
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
	ret->signals = g_hash_table_new (g_str_hash, g_str_equal);
	ret->methods = g_hash_table_new (g_str_hash, g_str_equal);
	ret->mutex = g_mutex_new ();
	ret->parent = NULL;
	xmms_object_ref (ret);

	return ret;
}

