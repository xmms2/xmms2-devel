#include "output.h"
#include "output_int.h"
#include "object.h"
#include "ringbuf.h"
#include "util.h"
#include "core.h"
#include "config_xmms.h"
#include "plugin.h"
#include "plugin_int.h"
#include "dbus_xmms.h"

#define xmms_output_lock(t) g_mutex_lock ((t)->mutex)
#define xmms_output_unlock(t) g_mutex_unlock ((t)->mutex)

static gpointer xmms_output_thread (gpointer data);

/*
 * Type definitions
 */

struct xmms_output_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;

	GMutex *mutex;
	GCond *cond;
	GThread *thread;
	gboolean running;

	guint played;
	gboolean is_open;

	guint samplerate;

	xmms_ringbuf_t *buffer;
	xmms_config_value_t *config;
	
	gpointer plugin_data;
};

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
xmms_output_config_string_get (xmms_output_t *output, gchar *val)
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

guint
xmms_output_samplerate_set (xmms_output_t *output, guint rate)
{
	g_return_val_if_fail (output, 0);
	g_return_val_if_fail (rate, 0);

	xmms_output_lock (output);
	if (rate != output->samplerate) {
		xmms_output_samplerate_set_method_t samplerate_method;
		samplerate_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET);
		output->samplerate = samplerate_method (output, rate);
	}
	xmms_output_unlock (output);
	return output->samplerate;
}

gboolean
xmms_output_open (xmms_output_t *output)
{
	xmms_output_open_method_t open_method;

	g_return_val_if_fail (output, FALSE);

	open_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_OPEN);

	if (!open_method || !open_method (output)) {
		XMMS_DBG ("Couldnt open output device");
		return FALSE;
	}

	output->is_open = TRUE;

	return TRUE;

}

void
xmms_output_close (xmms_output_t *output)
{
	xmms_output_close_method_t close_method;

	g_return_if_fail (output);

	close_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_CLOSE);

	if (!close_method)
		return;

	close_method (output);

	output->is_open = FALSE;

}

xmms_output_t *
xmms_output_new (xmms_plugin_t *plugin, xmms_config_data_t *config)
{
	xmms_output_t *output;
	
	g_return_val_if_fail (plugin, NULL);

	XMMS_DBG ("Trying to open output");

	output = g_new0 (xmms_output_t, 1);
	xmms_object_init (XMMS_OBJECT (output));
	output->plugin = plugin;
	output->mutex = g_mutex_new ();
	output->cond = g_cond_new ();
	output->buffer = xmms_ringbuf_new (32768);
	output->is_open = FALSE;
	
	xmms_output_set_config (output, config->output);
	
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

	output->played = 0;

}

static gpointer
xmms_output_thread (gpointer data)
{
	xmms_output_t *output;
	xmms_output_write_method_t write_method;

	output = (xmms_output_t*)data;
	g_return_val_if_fail (data, NULL);

	write_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_WRITE);
	g_return_val_if_fail (write_method, NULL);

	xmms_output_lock (output);
	while (output->running) {
		gchar buffer[4096];
		gint ret;

		xmms_ringbuf_wait_used (output->buffer, 4096, output->mutex);

		if (!output->is_open)
			xmms_output_open (output);

		ret = xmms_ringbuf_read (output->buffer, buffer, 4096);
		
		if (ret > 0) {

			xmms_output_unlock (output);
			write_method (output, buffer, ret);
			xmms_output_lock (output);

			output->played += ret;
			/** @todo some places we are counting in bytes,
			    in other in number of samples. Maybe we
			    want a xmms_sample_t and a XMMS_SAMPLE_SIZE */
			xmms_core_playtime_set ((guint)(output->played/(4.0f*output->samplerate/1000.0f)));
			
		} else if (xmms_ringbuf_eos (output->buffer)) {
			GTimeVal time;

			xmms_output_unlock (output);
			xmms_object_emit (XMMS_OBJECT (output), XMMS_DBUS_SIGNAL_OUTPUT_EOS_REACHED, NULL);
			xmms_output_lock (output);
			output->played = 0;
			
			while (xmms_ringbuf_eos (output->buffer)) {
				g_get_current_time (&time);
				g_time_val_add (&time, 10 * G_USEC_PER_SEC);
				if (!g_cond_timed_wait (output->cond, output->mutex, &time)){
					if (output->is_open) {
						XMMS_DBG ("Timed out");
						xmms_output_close (output);
					}
				}
			}

		}
	}
	xmms_output_unlock (output);

	/* FIXME: Cleanup */
	
	return NULL;
}
