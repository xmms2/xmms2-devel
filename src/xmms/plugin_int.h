#ifndef __XMMS_PLUGIN_INT_H__
#define __XMMS_PLUGIN_INT_H__

/*
 * Private functions
 */

gboolean xmms_plugin_init (gchar *path);
void xmms_plugin_scan_directory (const gchar *dir);

void xmms_plugin_ref (xmms_plugin_t *plugin);
void xmms_plugin_unref (xmms_plugin_t *plugin);

GList *xmms_plugin_list_get (xmms_plugin_type_t type);
void xmms_plugin_list_destroy (GList *list);

xmms_plugin_method_t xmms_plugin_method_get (xmms_plugin_t *plugin,
											 const gchar *member);


#endif
