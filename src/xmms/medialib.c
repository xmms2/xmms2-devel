/** @file
 *  This controls the medialib.
 */

#include "util.h"
#include "config_xmms.h"
#include "medialib.h"


xmms_plugin_t *
xmms_medialib_find_plugin (gchar *name)
{
	GList *list;
	xmms_plugin_t *plugin = NULL;

	g_return_val_if_fail (name, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_MEDIALIB);

	while (list) {
		plugin = (xmms_plugin_t *) list->data;
		if (g_strcasecmp (xmms_plugin_shortname_get (plugin), name) == 0)
			return plugin;

		list = g_list_next (list);
	}

	return NULL;
}

xmms_medialib_t *
xmms_medialib_init (xmms_plugin_t *plugin)
{
	xmms_medialib_new_method_t new_method;
	xmms_medialib_t *medialib;
	
	new_method = xmms_plugin_method_get (plugin, XMMS_METHOD_NEW);

	if (!new_method)
		return NULL;
	
	medialib = g_new0 (xmms_medialib_t, 1);

	medialib->plugin = plugin;
	medialib->mutex = g_mutex_new ();
	
	if (!new_method (medialib)) {
		XMMS_DBG ("new_method failed");
		g_mutex_free (medialib->mutex);
		g_free (medialib);
		return NULL;
	}
	
	return medialib;
}

void
xmms_medialib_set_data (xmms_medialib_t *medialib, gpointer data)
{

	g_return_if_fail (medialib);
	g_return_if_fail (data);

	medialib->data = data;

}

gpointer
xmms_medialib_get_data (xmms_medialib_t *medialib)
{
	g_return_val_if_fail (medialib, NULL);

	return medialib->data;

}

GList *
xmms_medialib_search (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry)
{
	xmms_medialib_search_method_t search;

	g_return_val_if_fail (medialib, NULL);
	g_return_val_if_fail (entry, NULL);

	search = xmms_plugin_method_get (medialib->plugin, XMMS_METHOD_SEARCH);

	g_return_val_if_fail (search, NULL);

	return (search (medialib, entry));
	
}

