#include "object.h"

void
xmms_object_init (xmms_object_t *object)
{
	g_return_if_fail (object);
	
	object->id = XMMS_OBJECT_MID;
	object->methods = g_hash_table_new (g_str_hash, g_str_equal);
}
				 
void
xmms_object_cleanup (xmms_object_t *object)
{
	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));

	g_hash_table_destroy (object->methods);
}

void
xmms_object_connect (xmms_object_t *object, const gchar *method,
					 xmms_object_handler_t handler)
{
	GList *list = NULL;
	gpointer key;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (method);
	g_return_if_fail (handler);

	if (g_hash_table_lookup_extended (object->methods, method, &key, (gpointer *)&list)) {
		list = g_list_prepend (list, handler);
		g_hash_table_insert (object->methods, key, list);
	} else {
		list = g_list_prepend (list, handler);
		g_hash_table_insert (object->methods, g_strdup (method), list);
	}
}

void
xmms_object_disconnect (xmms_object_t *object, const gchar *method,
						xmms_object_handler_t handler)
{
	GList *list = NULL;
	gpointer key;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (method);
	g_return_if_fail (handler);

	if (!g_hash_table_lookup_extended (object->methods, method, &key, (gpointer *)&list))
		return;
	if (!list)
		return;
	list = g_list_remove (list, handler);
	if (!list) {
		g_hash_table_remove (object->methods, key);
		g_free (key);
	} else {
		g_hash_table_insert (object->methods, key, list);
	}
}

void
xmms_object_emit (xmms_object_t *object, const gchar *method, gconstpointer data)
{
	GList *list, *node;

	g_return_if_fail (object);
	g_return_if_fail (XMMS_IS_OBJECT (object));
	g_return_if_fail (method);

	list = g_hash_table_lookup (object->methods, method);
	for (node = list; node; node = g_list_next (node)) {
		xmms_object_handler_t handler = node->data;

		if (handler)
			handler (object, data);
	}
}
