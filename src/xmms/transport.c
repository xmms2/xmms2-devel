#include "transport.h"
#include "util.h"

/*
 * Function prototypes
 */

static xmms_plugin_t *xmms_transport_find_plugin (const gchar *uri);

/*
 * Private functions
 */

xmms_transport_t *
xmms_transport_open (const gchar *uri)
{
	xmms_plugin_t *plugin;
	xmms_transport_t *transport;

	g_return_val_if_fail (uri, NULL);

	XMMS_DBG ("Trying to open stream: %s", uri);
	
	plugin = xmms_transport_find_plugin (uri);
	if (!plugin)
		return NULL;
	
	XMMS_DBG ("Found plugin: %s", xmms_plugin_name_get (plugin));

	transport = g_new0 (xmms_transport_t, 1);

	transport->plugin = plugin;
	
	return transport;
}

static xmms_plugin_t *
xmms_transport_find_plugin (const gchar *uri)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;
	xmms_transport_can_handle_method_t can_handle;

	g_return_val_if_fail (uri, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_TRANSPORT);

	for (node = list; node; node = g_list_next (node)) {
		plugin = node->data;
		can_handle = xmms_plugin_method_get (plugin, "can_handle");
		
		if (!can_handle)
			continue;

		if (can_handle (uri)) {
			xmms_plugin_ref (plugin);
			break;
		}
	}
	if (!node)
		plugin = NULL;

	if (list)
		xmms_plugin_list_destroy (list);

	return plugin;
}
