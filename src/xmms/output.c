#include "output.h"
#include "object.h"
#include "ringbuf.h"
#include "util.h"
#include "core.h"
#include "config_xmms.h"


#define xmms_output_lock(t) g_mutex_lock ((t)->mutex)
#define xmms_output_unlock(t) g_mutex_unlock ((t)->mutex)

static gpointer xmms_output_thread (gpointer data);

/*
 * Public functions
 */

gpointer
xmms_output_plugin_data_get (xmms_output_t *output)
{
	gpointer ret;
	g_return_val_if_fail (output, NULL);

	ret = output->plugin_data;

	return ret;
}

void
xmms_output_set_config (xmms_output_t *output, GHashTable *config)
{
	const gchar *sn;
	xmms_config_value_t *val;
	g_return_if_fail (output);
	g_return_if_fail (config);

	sn = xmms_plugin_shortname_get (output->plugin);

	val = xmms_config_value_lookup (config, sn);
	if (!val) {
	XMMS_DBG ("Adding section %s", sn);
		val = xmms_config_add_section (config, g_strdup (sn));
	}

	output->config = val;
}	

gchar *
xmms_output_get_config_string (xmms_output_t *output, gchar *val)
{
	xmms_config_value_t *value;

	g_return_val_if_fail (output, NULL);
		g_return_val_if_fail (val, NULL);
	
	value = xmms_config_value_list_lookup (output->config, val);

	return xmms_config_value_as_string (value);
}

void
xmms_output_plugin_data_set (xmms_output_t *output, gpointer data)
{
	output->plugin_data = data;
}

void
xmms_output_write (xmms_output_t *output, gpointer buffer, gint len)
{
	g_return_if_fail (output);
	g_return_if_fail (buffer);

	xmms_output_lock (output);
	xmms_ringbuf_wait_free (output->buffer, len, output->mutex);
	xmms_ringbuf_write (output->buffer, buffer, len);
	xmms_output_unlock (output);

}

/*
 * Private functions
 */

xmms_output_t *
xmms_output_open (xmms_plugin_t *plugin, xmms_config_data_t *config)
{
	xmms_output_t *output;
	xmms_output_open_method_t open_method;
	
	g_return_val_if_fail (plugin, NULL);

	XMMS_DBG ("Trying to open output");

	output = g_new0 (xmms_output_t, 1);
	xmms_object_init (XMMS_OBJECT (output));
	output->plugin = plugin;
	output->mutex = g_mutex_new ();
	output->cond = g_cond_new ();
	output->buffer = xmms_ringbuf_new (32768);
	
	xmms_output_set_config (output, config->output);
	
	open_method = xmms_plugin_method_get (plugin, XMMS_METHOD_OPEN);

	if (!open_method || !open_method (output)) {
		XMMS_DBG ("Couldnt open output device");
		xmms_ringbuf_destroy (output->buffer);
		g_mutex_free (output->mutex);
		g_free (output);
		return NULL;
	}

	return output;
}


xmms_plugin_t *
xmms_output_find_plugin (gchar *name)
{
	GList *list;
	xmms_plugin_t *plugin = NULL;

	g_return_val_if_fail (name, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_OUTPUT);

	while (list) {
		plugin = (xmms_plugin_t*) list->data;
		if (g_strcasecmp (xmms_plugin_shortname_get (plugin), name) == 0) {
			return plugin;
		}
		list = g_list_next (list);
	}

	return NULL;
}
	
void
xmms_output_start (xmms_output_t *output)
{
	g_return_if_fail (output);

	output->running = TRUE;
	output->thread = g_thread_create (xmms_output_thread, output, TRUE, NULL);
}

void
xmms_output_set_eos (xmms_output_t *output, gboolean eos)
{
	g_return_if_fail (output);
	
	xmms_ringbuf_set_eos (output->buffer, eos);
	if (!eos) {
		g_cond_signal (output->cond);
	}
}

static gpointer
xmms_output_thread (gpointer data)
{
	xmms_output_t *output;
	xmms_output_write_method_t write_method;
	guint played = 0;

	output = (xmms_output_t*)data;
	g_return_val_if_fail (data, NULL);

	XMMS_DBG ("Plugin %s", output->plugin->name);
	write_method = xmms_plugin_method_get (output->plugin, XMMS_METHOD_WRITE);
	g_return_val_if_fail (write_method, NULL);

	xmms_output_lock (output);
	while (output->running) {
		gchar buffer[4096];
		gint ret;

		xmms_ringbuf_wait_used (output->buffer, 4096, output->mutex);
		ret = xmms_ringbuf_read (output->buffer, buffer, 4096);
		
		if (ret > 0) {
			xmms_output_unlock (output);
			write_method (output, buffer, ret);
			xmms_output_lock (output);

			played += ret;
			xmms_core_playtime_set((guint)(played/(4.0f*44.1f)));
			
		} else if (xmms_ringbuf_eos (output->buffer)) {
			xmms_output_unlock (output);
			xmms_object_emit (XMMS_OBJECT (output), "eos-reached", NULL);
			xmms_output_lock (output);
			played = 0;
			if (xmms_ringbuf_eos (output->buffer))
				g_cond_wait (output->cond, output->mutex);
		}
	}
	xmms_output_unlock (output);

	/* FIXME: Cleanup */
	
	return NULL;
}
