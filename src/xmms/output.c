/**
 * @file
 * 
 */

#include "output.h"
#include "output_int.h"
#include "object.h"
#include "ringbuf.h"
#include "util.h"
#include "core.h"
#include "config_xmms.h"
#include "plugin.h"
#include "plugin_int.h"
#include "signal_xmms.h"

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
	guint open_samplerate;

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

gboolean
xmms_output_volume_get (xmms_output_t *output, gint *left, gint *right)
{
	xmms_output_volume_get_method_t vol;
	g_return_val_if_fail (output, FALSE);

	vol = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_MIXER_GET);
	g_return_val_if_fail (vol, FALSE);

	return vol (output, left, right);
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

xmms_config_value_t *
xmms_output_config_value_get (xmms_output_t *output, gchar *key, gchar *def)
{
	xmms_config_value_t *value;
	
	g_return_val_if_fail (output, NULL);
	g_return_val_if_fail (key, NULL);
	
	value = xmms_config_value_list_lookup (output->config, key);

	if (!value) {
		value = xmms_config_value_create (XMMS_CONFIG_VALUE_PLAIN, key);
		xmms_config_value_data_set (value, g_strdup (def));
		xmms_config_value_list_add (output->config, value);
	}

	return value;
}


/**
 * Retrieve a config variable for a certain output section as a string.
 * If the requested value isn't available NULL is returned. 
 *
 * @param output The output structure so we know where in the config to look
 * @param val The config variable to inquire
 * @return a gchar with the config data, or NULL if not available
 */
gchar *
xmms_output_config_string_get (xmms_output_t *output, gchar *val)
{
	xmms_config_value_t *value;
	
	g_return_val_if_fail (output, NULL);
	g_return_val_if_fail (val, NULL);
	
	value = xmms_config_value_list_lookup (output->config, val);
	
	return xmms_config_value_as_string (value);
}


/**
 * Retrieve a config variable for a certain output section as an integer. 
 * If the requested value isn't available 0 is returned. 
 *
 * @param output The output structure so we know where in the config to look
 * @param val The config variable to inquire
 * @return a gint with the config data, or 0 if not available
 */
gint 
xmms_output_config_int_get (xmms_output_t *output, gchar *val)
{
	xmms_config_value_t *value;
	
	g_return_val_if_fail (output, 0);
	g_return_val_if_fail (val, 0);
	
	value = xmms_config_value_list_lookup (output->config, val);
	
	return xmms_config_value_as_int (value);
}


void
xmms_output_plugin_data_set (xmms_output_t *output, gpointer data)
{
	output->plugin_data = data;
}

/*
 * Private functions
 */


/**
 * @internal
 */
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

guint
xmms_output_samplerate_set (xmms_output_t *output, guint rate)
{
	g_return_val_if_fail (output, 0);
	g_return_val_if_fail (rate, 0);

	if (!output->is_open) {
		output->open_samplerate = rate;
		return rate;
	}

	if (rate != output->samplerate) {
		xmms_output_samplerate_set_method_t samplerate_method;
		samplerate_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET);
		output->samplerate = samplerate_method (output, rate);
		XMMS_DBG ("samplerate set to: %d", output->samplerate);
	}
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

	XMMS_DBG ("opening with samplerate: %d",output->open_samplerate);
	xmms_output_samplerate_set (output, output->open_samplerate);

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
	xmms_output_new_method_t new;
	
	g_return_val_if_fail (plugin, NULL);

	XMMS_DBG ("Trying to open output");

	output = g_new0 (xmms_output_t, 1);
	xmms_object_init (XMMS_OBJECT (output));
	output->plugin = plugin;
	output->mutex = g_mutex_new ();
	output->cond = g_cond_new ();
	output->buffer = xmms_ringbuf_new (32768);
	output->is_open = FALSE;
	output->open_samplerate = 44100;
	
	xmms_output_set_config (output, config->output);

	new = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);

	if (!new (output)) {
		g_mutex_free (output->mutex);
		g_cond_free (output->cond);
		xmms_ringbuf_destroy (output->buffer);
		g_free (output);
		return NULL;
	}
	
	return output;
}

void
xmms_output_flush (xmms_output_t *output)
{
	xmms_output_flush_method_t flush;

	g_return_if_fail (output);
	
	flush = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_FLUSH);
	g_return_if_fail (flush);

	g_mutex_lock (output->mutex);

	xmms_ringbuf_clear (output->buffer);

	flush (output);

	g_mutex_unlock (output->mutex);
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

void
xmms_output_played_samples_set (xmms_output_t *output, guint samples)
{
	g_return_if_fail (output);
	g_mutex_lock (output->mutex);
	output->played = samples * 4;
	XMMS_DBG ("Set played to %d", output->played);
	g_mutex_unlock (output->mutex);
}

static gpointer
xmms_output_thread (gpointer data)
{
	xmms_output_t *output;
	xmms_output_buffersize_get_method_t buffersize_get_method;
	xmms_output_write_method_t write_method;

	output = (xmms_output_t*)data;
	g_return_val_if_fail (data, NULL);

	write_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_WRITE);
	g_return_val_if_fail (write_method, NULL);

	buffersize_get_method = xmms_plugin_method_get (output->plugin, XMMS_PLUGIN_METHOD_BUFFERSIZE_GET);

	xmms_output_lock (output);
	while (output->running) {
		gchar buffer[4096];
		gint ret;

		xmms_ringbuf_wait_used (output->buffer, 4096, output->mutex);

		if (!output->is_open)
			xmms_output_open (output);

		ret = xmms_ringbuf_read (output->buffer, buffer, 4096);
		
		if (ret > 0) {
			guint played_time;
			xmms_output_unlock (output);
			write_method (output, buffer, ret);
			xmms_output_lock (output);

			output->played += ret;
			/** @todo some places we are counting in bytes,
			    in other in number of samples. Maybe we
			    want a xmms_sample_t and a XMMS_SAMPLE_SIZE */
			
			played_time = (guint)(output->played/(4.0f*output->samplerate/1000.0f));

			if (buffersize_get_method) {
				guint buffersize = buffersize_get_method (output);
				buffersize = buffersize/(2.0f*output->samplerate/1000.0f);
/*				XMMS_DBG ("buffer: %dms", buffersize);*/

				if (played_time >= buffersize) {
					played_time -= buffersize;
				} else {
					played_time = 0;
				}
			}
			xmms_core_playtime_set (played_time);

			
		} else if (xmms_ringbuf_iseos (output->buffer)) {
			GTimeVal time;

			xmms_output_unlock (output);
			xmms_object_emit (XMMS_OBJECT (output), XMMS_SIGNAL_OUTPUT_EOS_REACHED, NULL);
			xmms_output_lock (output);
			output->played = 0;
			
			while (xmms_ringbuf_iseos (output->buffer)) {
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
