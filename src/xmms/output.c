#include "output.h"
#include "ringbuf.h"

struct xmms_output_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;

	GMutex *mutex;
	GThread *thread;
	gboolean running;

	xmms_ringbuf_t *buffer;
	
	gpointer plugin_data;
};

#define xmms_output_lock(t) g_mutex_lock ((t)->mutex)
#define xmms_output_unlock(t) g_mutex_unlock ((t)->mutex)

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

	if (!open_method || !open_method (output)) {
		XMMS_DBG ("Couldnt open output device");
		xmms_ringbuf_destroy (output->ringbuf);
		g_mutex_free (output->mutex);
		g_free (output);
	}

	return output;
}
	
void
xmms_output_start (xmms_output_t *output)
{
	g_return_if_fail (output);

	output->running = TRUE;
	output->thread = g_thread_create (xmms_output_thread, output, TRUE, NULL);
}

void
xmms_output_write (xmms_output_t *output, gint ret)
{
	g_return_if_fail (output);
	g_return_if_fail (ret);

	xmms_output_lock (output);
	xmms_ringbuf_wait_free (output->buffer, sizeof(gint), output->mutex);
	xmms_ringbuf_write (output->buffer, G_INT_TO_POINTER (ret), sizeof (gint));
	xmms_output_unlock (output);

}

gint
xmms_output_read (xmms_output_t *output, gchar *buffer, gint len)
{
	gint ret;

	g_return_val_if_fail (output, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (len > 0, -1);

	xmms_output_lock (output);
	xmms_ringbuf_wait_used (output->buffer, len, output->mutex);
	xmms_ringbuf_read (output->buffer, buffer, len);
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

	write_method = xmms_plugin_method_get (output->plugin, "write");
	g_return_val_if_fail (write_method, NULL);

	while (output->running) {
		gchar buffer[512];
		xmms_output_lock (output);

		ret = xmms_output_read (output, buffer, 512);
		
		if (ret > 0) {
			xmms_ringbuf_wait_free (output->buffer, ret, output->mutex);
			write_method (output, buffer, ret);
		} else {
			XMMS_DBG ("Not good?!");
		}

		xmms_transport_unlock (output);
	}

	return NULL;
	
}
