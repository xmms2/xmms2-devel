#include "output.h"
#include "object.h"
#include "ringbuf.h"
#include "util.h"

#define xmms_output_lock(t) g_mutex_lock ((t)->mutex)
#define xmms_output_unlock(t) g_mutex_unlock ((t)->mutex)

static gpointer xmms_output_thread (gpointer data);

gpointer
xmms_output_plugin_data_get (xmms_output_t *output)
{
	g_return_val_if_fail (output, NULL);

	return output->plugin_data;
}

void
xmms_output_plugin_data_set (xmms_output_t *output, gpointer data)
{
	output->plugin_data = data;
}

xmms_output_t *
xmms_output_open (xmms_plugin_t *plugin)
{
	xmms_output_t *output;
	xmms_output_open_method_t open_method;
	
	g_return_val_if_fail (plugin, NULL);

	XMMS_DBG ("Trying to open output");

	output = g_new0 (xmms_output_t, 1);
	xmms_object_init (XMMS_OBJECT (output));
	output->plugin = plugin;
	output->mutex = g_mutex_new ();
	output->buffer = xmms_ringbuf_new (32768);
	
	open_method = xmms_plugin_method_get (plugin, "open");

	if (!open_method || !open_method (output, "/tmp/foo")) {
		XMMS_DBG ("Couldnt open output device");
		xmms_ringbuf_destroy (output->buffer);
		g_mutex_free (output->mutex);
		g_free (output);
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
xmms_output_write (xmms_output_t *output, gpointer buffer, gint len)
{
	g_return_if_fail (output);
	g_return_if_fail (buffer);

	xmms_output_lock (output);
	xmms_ringbuf_wait_free (output->buffer, len, output->mutex);
	xmms_ringbuf_write (output->buffer, buffer, len);
	xmms_output_unlock (output);

}

gint
xmms_output_read (xmms_output_t *output, gchar *buffer, gint len)
{
	gint ret;

	g_return_val_if_fail (output, -1);
	g_return_val_if_fail (buffer, -1);

	xmms_output_lock (output);
	xmms_ringbuf_wait_used (output->buffer, len, output->mutex);
	ret = xmms_ringbuf_read (output->buffer, buffer, len);
	xmms_output_unlock (output);

	return ret;
}

static gpointer
xmms_output_thread (gpointer data)
{
	xmms_output_t *output;
	xmms_output_write_method_t write_method;

	output = (xmms_output_t*)data;
	g_return_val_if_fail (data, NULL);

	XMMS_DBG ("Plugin %s", output->plugin->name);
	write_method = xmms_plugin_method_get (output->plugin, "write");
	g_return_val_if_fail (write_method, NULL);

	while (output->running) {
		gchar buffer[4096];
		gint ret;

		ret = xmms_output_read (output, buffer, 4096);
		
		if (ret > 0) {
		/*	xmms_ringbuf_wait_free (output->buffer, ret, output->mutex);*/
			write_method (output, buffer, ret);
		} else {
			XMMS_DBG ("Not good?!");
		}


	}

	return NULL;
	
}
