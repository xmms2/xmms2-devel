#include "object.h"
#include "util.h"

typedef struct {
	xmms_object_handler_t handler;
	gpointer userdata;
} xmms_object_handler_entry_t;

void
xmms_object_init (xmms_object_t *object)
{
	g_return_if_fail (object);
	
	object->id = XMMS_OBJECT_MID;
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

	g_hash_table_foreach_remove (object->methods, xmms_object_cleanup_foreach, NULL);
	g_hash_table_destroy (object->methods);
	g_mutex_free (object->mutex);
}

void
xmms_object_connect (xmms_object_t *object, const gchar *method,
					 xmms_object_handler_t handler, gpointer userdata)
{
	GList *list = NULL;
	gpointer key;
	gpointer val;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (method);
	g_return_if_fail (handler);

	
	entry = g_new0 (xmms_object_handler_entry_t, 1);
	entry->handler = handler;
	entry->userdata = userdata;

/*	g_mutex_lock (object->mutex);*/
	
	if (g_hash_table_lookup_extended (object->methods, method, &key, &val)) {
		list = g_list_prepend (val, entry);
		g_hash_table_insert (object->methods, key, list);
	} else {
		list = g_list_prepend (list, entry);
		g_hash_table_insert (object->methods, g_strdup (method), list);
	}

/*	g_mutex_unlock (object->mutex);*/
}

void
xmms_object_disconnect (xmms_object_t *object, const gchar *method,
						xmms_object_handler_t handler)
{
	GList *list = NULL, *node;
	gpointer key;
	gpointer val;
	xmms_object_handler_entry_t *entry;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (method);
	g_return_if_fail (handler);

	g_mutex_lock (object->mutex);
	
	if (!g_hash_table_lookup_extended (object->methods, method, &key, &val))
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
		g_hash_table_remove (object->methods, key);
		g_free (key);
	} else {
		g_hash_table_insert (object->methods, key, list);
	}
unlock:
	g_mutex_unlock (object->mutex);
}

void
xmms_object_emit (xmms_object_t *object, const gchar *method, gconstpointer data)
{
	GList *list, *node, *list2 = NULL;
	xmms_object_handler_entry_t *entry;
	
	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (method);

	g_mutex_lock (object->mutex);
	
	list = g_hash_table_lookup (object->methods, method);
	for (node = list; node; node = g_list_next (node)) {
		entry = node->data;

		list2 = g_list_prepend (list2, entry);
	}

	if (!list2) { /* no method -> send to parent */
		if (object->parent) {
			xmms_object_emit (object->parent, method, data);
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
