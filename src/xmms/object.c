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
#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/playlist_entry.h"

typedef struct {
	xmms_object_handler_t handler;
	gpointer userdata;
} xmms_object_handler_entry_t;

xmms_object_method_arg_t *
xmms_object_arg_new (xmms_object_method_arg_type_t type,
		     gpointer val)
{
	xmms_object_method_arg_t *ret;

	ret = g_new0 (xmms_object_method_arg_t, 1);
	switch (type) {
		case XMMS_OBJECT_METHOD_ARG_UINT32:
			ret->retval.uint32 = GPOINTER_TO_UINT (val);
			break;
		case XMMS_OBJECT_METHOD_ARG_INT32:
			ret->retval.int32 = GPOINTER_TO_INT (val);
			break;
		case XMMS_OBJECT_METHOD_ARG_STRING:
			ret->retval.string = (gchar*) val;
			break;
		case XMMS_OBJECT_METHOD_ARG_PLAYLIST_ENTRY:
			ret->retval.playlist_entry = (xmms_playlist_entry_t *)val;
			break;
		case XMMS_OBJECT_METHOD_ARG_UINTLIST:
			ret->retval.uintlist = (GList*)val;
			break;
		case XMMS_OBJECT_METHOD_ARG_INTLIST:
			ret->retval.intlist = (GList*)val;
			break;
		case XMMS_OBJECT_METHOD_ARG_STRINGLIST:
			ret->retval.stringlist = (GList*)val;
			break;
		case XMMS_OBJECT_METHOD_ARG_NONE:
			break;
	}
	ret->rettype = type;

	return ret;
}

void
xmms_object_init (xmms_object_t *object)
{
	g_return_if_fail (object);
	
	object->id = XMMS_OBJECT_MID;
	object->signals = g_hash_table_new (g_str_hash, g_str_equal);
	object->methods = g_hash_table_new (g_str_hash, g_str_equal);
	object->mutex = g_mutex_new ();
	object->parent = NULL;
}

void
xmms_object_parent_set (xmms_object_t *object, xmms_object_t *parent)
{
	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));

	g_mutex_lock (object->mutex);
	object->parent = parent;
	g_mutex_unlock (object->mutex);
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

/*	g_mutex_lock (object->mutex);*/
	
	if (g_hash_table_lookup_extended (object->signals, signal, &key, &val)) {
		list = g_list_prepend (val, entry);
		g_hash_table_insert (object->signals, key, list);
	} else {
		list = g_list_prepend (list, entry);
		g_hash_table_insert (object->signals, g_strdup (signal), list);
	}

/*	g_mutex_unlock (object->mutex);*/
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

	g_mutex_lock (object->mutex);
	
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
	g_mutex_unlock (object->mutex);
}

void
xmms_object_emit (xmms_object_t *object, const gchar *signal, gconstpointer data)
{
	GList *list, *node, *list2 = NULL;
	xmms_object_handler_entry_t *entry;
	
	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (signal);

	g_mutex_lock (object->mutex);
	
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

	g_mutex_unlock (object->mutex);

	for (node = list2; node; node = g_list_next (node)) {
		entry = node->data;
		
		if (entry && entry->handler)
			entry->handler (object, data, entry->userdata);
	}
	g_list_free (list2);

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
