#include "xmms/plugin.h"

/*
 * Function prototypes
 */

static gboolean xmms_file_can_handle (const gchar *uri);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "File transport " VERSION,
							  "Plain file transport plugin");
	
	xmms_plugin_method_add (plugin, "can_handle", xmms_file_can_handle);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_file_can_handle (const gchar *uri)
{
	g_return_val_if_fail (uri, FALSE);

	if ((g_strncasecmp (uri, "file:", 5) == 0) || (uri[0] == '/'))
		return TRUE;
	return FALSE;
}
