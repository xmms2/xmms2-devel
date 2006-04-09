
#include "xmmspriv/xmms_outputplugin.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmms/xmms_log.h"

struct xmms_output_plugin_St {
	xmms_plugin_t plugin;

	xmms_output_methods_t methods;
};


static void
xmms_output_plugin_destroy (xmms_object_t *obj)
{
	/*xmms_output_plugin_t *plugin = (xmms_output_plugin_t *)obj;*/

	xmms_plugin_destroy ((xmms_plugin_t *)obj);
}

xmms_plugin_t *
xmms_output_plugin_new (void)
{
	xmms_output_plugin_t *res;

	res = xmms_object_new (xmms_output_plugin_t, xmms_output_plugin_destroy);

	return (xmms_plugin_t *)res;
}

void
xmms_output_plugin_methods_set (xmms_output_plugin_t *plugin, xmms_output_methods_t *methods)
{
	g_return_if_fail (plugin);
	g_return_if_fail (plugin->plugin.type == XMMS_PLUGIN_TYPE_OUTPUT);

	XMMS_DBG ("Registering output '%s'", xmms_plugin_shortname_get ((xmms_plugin_t *)plugin));

	memcpy (&plugin->methods, methods, sizeof (xmms_output_methods_t));
}

gboolean
xmms_output_plugin_verify (xmms_plugin_t *_plugin)
{
	xmms_output_plugin_t *plugin = (xmms_output_plugin_t *)_plugin;
	gboolean w, s, o, c;

	g_return_val_if_fail (plugin, FALSE);
	g_return_val_if_fail (_plugin->type == XMMS_PLUGIN_TYPE_OUTPUT, FALSE);

	if (!(plugin->methods.new &&
	      plugin->methods.destroy &&
	      plugin->methods.flush)) {
		return FALSE;
	}

	w = !!plugin->methods.write;
	s = !!plugin->methods.status;

	if (!(!w ^ !s)) {
		return FALSE;
	}

	o = !!plugin->methods.open;
	c = !!plugin->methods.close;

	/* 'write' plugins need these two methods, 'status' plugins may
	 * have neither of them
	 */
	return (w && o && c) || (s && !o && !c);
}

xmms_config_property_t *
xmms_output_plugin_config_property_register (xmms_output_plugin_t *plugin, const gchar *name, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata)
{
	return xmms_plugin_config_property_register ((xmms_plugin_t *)plugin, name, default_value, cb, userdata);

}

gboolean
xmms_output_plugin_method_new (xmms_output_plugin_t *plugin, xmms_output_t *output)
{
	gboolean ret = TRUE;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.new) {
		ret = plugin->methods.new (output);
	}

	return ret;
}

void
xmms_output_plugin_method_destroy (xmms_output_plugin_t *plugin, xmms_output_t *output)
{
	g_return_if_fail (output);
	g_return_if_fail (plugin);

	if (plugin->methods.destroy) {
		plugin->methods.destroy (output);
	}
}

gboolean
xmms_output_plugin_method_open (xmms_output_plugin_t *plugin, xmms_output_t *output)
{
	gboolean ret = TRUE;
	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.open) {
		ret = plugin->methods.open (output);
	}

	return ret;
}

void
xmms_output_plugin_method_close (xmms_output_plugin_t *plugin, xmms_output_t *output)
{
	g_return_if_fail (output);
	g_return_if_fail (plugin);

	if (plugin->methods.close) {
		plugin->methods.close (output);
	}
}

void
xmms_output_plugin_method_flush (xmms_output_plugin_t *plugin, xmms_output_t *output)
{
	g_return_if_fail (output);
	g_return_if_fail (plugin);

	if (plugin->methods.flush) {
		plugin->methods.flush (output);
	}
}

gboolean
xmms_output_plugin_method_format_set (xmms_output_plugin_t *plugin, xmms_output_t *output, xmms_stream_type_t *st)
{
	gboolean res = TRUE;
	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.format_set) {
		res = plugin->methods.format_set (output, st);
	}

	return res;
}


gboolean
xmms_output_plugin_method_status_available (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return !!plugin->methods.status;
}

gboolean
xmms_output_plugin_method_status (xmms_output_plugin_t *plugin, xmms_output_t *output, int st)
{
	gboolean res = TRUE;
	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.status) {
		res = plugin->methods.status (output, st);
	}
	return res;
}



guint
xmms_output_plugin_method_latency_get (xmms_output_plugin_t *plugin, xmms_output_t *output)
{
	guint ret = 0;
	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.latency_get) {
		ret = plugin->methods.latency_get (output);
	}

	return ret;
}

gboolean
xmms_output_plugin_method_volume_set_available (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return !!plugin->methods.volume_set;
}

gboolean
xmms_output_plugin_methods_volume_set (xmms_output_plugin_t *plugin, xmms_output_t *output, const gchar *chan, guint val)
{
	gboolean res = FALSE;
	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.volume_set) {
		res = plugin->methods.volume_set (output, chan, val);
	}
	return res;

}

gboolean
xmms_output_plugin_method_volume_get_available (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return !!plugin->methods.volume_get;
}

gboolean
xmms_output_plugin_method_volume_get (xmms_output_plugin_t *plugin, xmms_output_t *output, const gchar **n, guint *x, guint *y)
{
	gboolean res = FALSE;
	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.volume_get) {
		res = plugin->methods.volume_get (output, n, x, y);
	}

	return res;
}


void
xmms_output_plugin_method_write (xmms_output_plugin_t *plugin, xmms_output_t *output, gpointer buf, gint len, xmms_error_t *err)
{
	g_return_if_fail (output);
	g_return_if_fail (plugin);
	g_return_if_fail (plugin->methods.latency_get);

	plugin->methods.write (output, buf, len, err);
}

