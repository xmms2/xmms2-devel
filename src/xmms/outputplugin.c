/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include "xmmspriv/xmms_outputplugin.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmms/xmms_log.h"

struct xmms_output_plugin_St {
	xmms_plugin_t plugin;

	xmms_output_methods_t methods;

	/* make sure we only do one call at a time */
	GMutex *api_mutex;

	/* */
	xmms_playback_status_t write_status;
	gboolean write_running;
	GMutex *write_mutex;
	GCond *write_cond;
	GThread *write_thread;
	xmms_output_t *write_output;
};

static gboolean xmms_output_plugin_writer_status (xmms_output_plugin_t *plugin,
                                                  xmms_output_t *output,
                                                  xmms_playback_status_t s);
static gpointer xmms_output_plugin_writer (gpointer data);


/**
 * Output plugin object destructor.
 * @param obj an output plugin object
 */
static void
xmms_output_plugin_destroy (xmms_object_t *obj)
{
	xmms_output_plugin_t *plugin = (xmms_output_plugin_t *)obj;

	g_mutex_free (plugin->api_mutex);
	g_mutex_free (plugin->write_mutex);
	g_cond_free (plugin->write_cond);

	xmms_plugin_destroy ((xmms_plugin_t *)obj);
}


/**
 * Output plugin object constructor.
 * @return a new output plugin object.
 */
xmms_plugin_t *
xmms_output_plugin_new (void)
{
	xmms_output_plugin_t *res;

	res = xmms_object_new (xmms_output_plugin_t, xmms_output_plugin_destroy);
	res->api_mutex = g_mutex_new ();
	res->write_mutex = g_mutex_new ();
	res->write_cond = g_cond_new ();

	return (xmms_plugin_t *)res;
}


/**
 * Register the output plugin methods.
 * @param plugin an output plugin object
 * @param methods a struct pointing to the plugin specific methods
 */
void
xmms_output_plugin_methods_set (xmms_output_plugin_t *plugin,
                                xmms_output_methods_t *methods)
{
	g_return_if_fail (plugin);
	g_return_if_fail (plugin->plugin.type == XMMS_PLUGIN_TYPE_OUTPUT);

	XMMS_DBG ("Registering output '%s'",
	          xmms_plugin_shortname_get ((xmms_plugin_t *)plugin));

	memcpy (&plugin->methods, methods, sizeof (xmms_output_methods_t));
}


/**
 * Verify output plugin methods.
 * Each output plugin must implement at least _new, _destroy and _flush.
 * Depending on what kind of mechanism the output plugin uses it will
 * implement _write, _open, _close, or in the case of event driven
 * plugins, _status. An event driven plugin is not allowed to not
 * implement _open, _close.
 * @param _plugin an output plugin object.
 * @return TRUE if the plugin has the required methods, else FALSE
 */
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
		XMMS_DBG ("Missing: new, destroy or flush!");
		return FALSE;
	}

	w = !!plugin->methods.write;
	s = !!plugin->methods.status;

	if (!(!w ^ !s)) {
		XMMS_DBG ("Neither write or status based.");
		return FALSE;
	}

	o = !!plugin->methods.open;
	c = !!plugin->methods.close;

	if (w) {
		/* 'write' type. */
		if (!(o && c)) {
			XMMS_DBG ("Write type misses open or close.");
			return FALSE;
		}
	} else {
		/* 'self driving' type */
		if (o || c) {
			XMMS_DBG ("Status type has open or close.");
			return FALSE;
		}
	}

	return TRUE;
}


/**
 * Register a configuration directive.
 * As an optional, but recomended functionality the plugin can decide to
 * subscribe on the configuration value and will thus be notified when it
 * changes by passing a callback, and if needed, userdata.
 * @param plugin an output plugin object
 * @param name the name of the configuration directive
 * @param default_value the default value of the configuration directive
 * @param cb the method to call on configuration value changes
 * @param userdata a user specified variable to be passed to the callback
 * @return a configuration structure based on the given input.
 */
xmms_config_property_t *
xmms_output_plugin_config_property_register (xmms_output_plugin_t *plugin,
                                             const gchar *name,
                                             const gchar *default_value,
                                             xmms_object_handler_t cb,
                                             gpointer userdata)
{
	xmms_plugin_t *p = (xmms_plugin_t *) plugin;

	return xmms_plugin_config_property_register (p, name, default_value,
	                                             cb, userdata);
}


/**
 * Initiate the output plugin.
 * This calls the xmms_foo_new method in the output plugin which should
 * setup all the basics needed before opening the device. If the plugin
 * fails to initiate itself its destructor will automatically be called.
 * If the plugin is not event driven a writer thread will be started.
 * @param plugin an output plugin object
 * @param output an output object
 * @return TRUE on success, FALSE on failure
 */
gboolean
xmms_output_plugin_method_new (xmms_output_plugin_t *plugin,
                               xmms_output_t *output)
{
	gboolean ret = TRUE;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.new) {
		ret = plugin->methods.new (output);
	}

	if (ret && !plugin->methods.status) {
		plugin->write_running = TRUE;
		plugin->write_thread = g_thread_create (xmms_output_plugin_writer,
		                                        plugin, TRUE, NULL);
		plugin->write_status = XMMS_PLAYBACK_STATUS_STOP;
	}

	return ret;
}


/**
 * Destroy the output plugin.
 * Call the output plugin destructor. If the output plugin is not event
 * driven, the running writer thread will be stopped.
 * @param plugin an output plugin object
 * @param output an output object
 */
void
xmms_output_plugin_method_destroy (xmms_output_plugin_t *plugin,
                                   xmms_output_t *output)
{
	g_return_if_fail (output);
	g_return_if_fail (plugin);

	if (plugin->write_thread) {
		plugin->write_running = FALSE;
		g_cond_signal (plugin->write_cond);
		g_thread_join (plugin->write_thread);
		plugin->write_thread = NULL;
	}

	if (plugin->methods.destroy) {
		g_mutex_lock (plugin->api_mutex);
		plugin->methods.destroy (output);
		g_mutex_unlock (plugin->api_mutex);
	}
}


/**
 * Flush the output plugin.
 * When the flush method in the output plugin is run the audio buffer on the
 * soundcard is cleared.
 * @param plugin an output plugin object
 * @param output an output object
 */
void
xmms_output_plugin_method_flush (xmms_output_plugin_t *plugin,
                                 xmms_output_t *output)
{
	g_return_if_fail (output);
	g_return_if_fail (plugin);

	if (plugin->methods.flush) {
		g_mutex_lock (plugin->api_mutex);
		plugin->methods.flush (output);
		g_mutex_unlock (plugin->api_mutex);
	}
}


/**
 * Check if the output plugin always needs notified of track changes.
 * Some output plugins may want to do crazy stuff like embedding metadata
 * into the output stream. These plugins will use the format_set_always
 * instead of format_set, and are thus guaranteed to be notified even if the
 * format of two songs are equal.
 * @param plugin an output plugin object
 * @return TRUE if the output plugin always needs to set format, else FALSE
 */
gboolean
xmms_output_plugin_format_set_always (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.format_set_always) {
		return TRUE;
	}
	return FALSE;
}


/**
 * Set the output format.
 * This method may be called on either track change, or on format changes
 * depending on how the output plugin rolls. The regular plugins that use
 * format_set should let the soundcard drain the buffer before changing the
 * format to avoid chopped off songs.
 * @param plugin an output plugin object
 * @param output an output object
 * @return TRUE if the format was successfully set.
 */
gboolean
xmms_output_plugin_method_format_set (xmms_output_plugin_t *plugin,
                                      xmms_output_t *output,
                                      xmms_stream_type_t *st)
{
	gboolean res = TRUE;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.format_set) {
		g_mutex_lock (plugin->api_mutex);
		res = plugin->methods.format_set (output, st);
		g_mutex_unlock (plugin->api_mutex);
	} else if (plugin->methods.format_set_always) {
		g_mutex_lock (plugin->api_mutex);
		res = plugin->methods.format_set_always (output, st);
		g_mutex_unlock (plugin->api_mutex);
	}

	return res;
}


/**
 * Update the output plugin with the current playback status.
 * This is interesting for event driven output plugins.
 * @param plugin an output plugin object
 * @param output an output object
 * @param st the new playback status
 */
gboolean
xmms_output_plugin_method_status (xmms_output_plugin_t *plugin,
                                  xmms_output_t *output, gint st)
{
	gboolean res = TRUE;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.status) {
		res = plugin->methods.status (output, st);
	} else if (plugin->write_thread) {
		XMMS_DBG ("Running status changed... %d", st);
		res = xmms_output_plugin_writer_status (plugin, output, st);
	}
	return res;
}


/**
 * Get the number of bytes in the soundcard buffer.
 * This is needed for the visualization to perform correct synchronization
 * between audio and graphics for example.
 * @param plugin an output plugin object
 * @param output an output object
 * @return the current number of bytes in the soundcard buffer or 0 on failure
 */
guint
xmms_output_plugin_method_latency_get (xmms_output_plugin_t *plugin,
                                       xmms_output_t *output)
{
	guint ret = 0;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.latency_get) {
		ret = plugin->methods.latency_get (output);
	}

	return ret;
}


/**
 * Check if an output plugin can change the volume.
 * @param plugin an output plugin object.
 * @return TRUE if the volume has a volume_set method, else FALSE
 */
gboolean
xmms_output_plugin_method_volume_set_available (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return !!plugin->methods.volume_set;
}


/**
 * Set volume.
 * @param plugin an output plugin object
 * @param output an output object
 * @param chan the name of the channel to set volume on
 * @param val the volume level to set
 * @return TRUE if the update was successful, else FALSE
 */
gboolean
xmms_output_plugin_methods_volume_set (xmms_output_plugin_t *plugin,
                                       xmms_output_t *output,
                                       const gchar *chan, guint val)
{
	gboolean res = FALSE;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.volume_set) {
		res = plugin->methods.volume_set (output, chan, val);
	}

	return res;
}


/**
 * Check if an output plugin can get the volume level.
 * @param plugin an output plugin object
 * @return TRUE if the output plugin can get the volume level, else FALSE
 */
gboolean
xmms_output_plugin_method_volume_get_available (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return !!plugin->methods.volume_get;
}


/**
 * Get volume.
 * This method is typically called twice. The first run NULL will be passed
 * to param n and x, and the output plugin will then set the number of
 * available channels to y and return TRUE. When the channels are known
 * memory will be allocated for the channel names and volume level lists
 * and the method will be called again, and this time the volume levels
 * are extracted for real.
 * @param plugin an output plugin object
 * @param output an output object
 * @param n a pointer to a list that is to be filled with channel names
 * @param x a pointer to a list that is to be filled with volume levels
 * @param y a pointer to a list that is to be filled with the number of channels
 * @return TRUE if the volume/channel count successfully retrieved, else FALSE
 */
gboolean
xmms_output_plugin_method_volume_get (xmms_output_plugin_t *plugin,
                                      xmms_output_t *output,
                                      const gchar **n, guint *x, guint *y)
{
	gboolean res = FALSE;

	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.volume_get) {
		res = plugin->methods.volume_get (output, n, x, y);
	}

	return res;
}


/* Used when we have to drive the output... */

static gboolean
xmms_output_plugin_writer_status (xmms_output_plugin_t *plugin,
                                  xmms_output_t *output,
                                  xmms_playback_status_t status)
{
	g_mutex_lock (plugin->write_mutex);
	plugin->write_status = status;
	plugin->write_output = output;
	g_cond_signal (plugin->write_cond);
	g_mutex_unlock (plugin->write_mutex);

	return TRUE;
}


static gpointer
xmms_output_plugin_writer (gpointer data)
{
	xmms_output_plugin_t *plugin = (xmms_output_plugin_t *) data;
	xmms_output_t *output = NULL;
	gchar buffer[4096];
	gint ret;

	g_mutex_lock (plugin->write_mutex);

	while (plugin->write_running) {
		if (plugin->write_status == XMMS_PLAYBACK_STATUS_STOP) {
			if (output) {
				g_mutex_lock (plugin->api_mutex);
				plugin->methods.close (output);
				g_mutex_unlock (plugin->api_mutex);
				output = NULL;
			}
			g_cond_wait (plugin->write_cond, plugin->write_mutex);
		} else if (plugin->write_status == XMMS_PLAYBACK_STATUS_PAUSE) {
			xmms_config_property_t *p;
			p = xmms_config_lookup ("output.flush_on_pause");
			if (xmms_config_property_get_int (p)) {
				g_mutex_lock (plugin->api_mutex);
				plugin->methods.flush (output);
				g_mutex_unlock (plugin->api_mutex);
			}
			g_cond_wait (plugin->write_cond, plugin->write_mutex);
		} else if (plugin->write_status == XMMS_PLAYBACK_STATUS_PLAY) {
			if (!output) {
				output = plugin->write_output;
				g_mutex_lock (plugin->api_mutex);
				if (!plugin->methods.open (output)) {
					XMMS_DBG ("Couldn't open output");
					plugin->write_status = XMMS_PLAYBACK_STATUS_STOP;
					output = NULL;
				}
				g_mutex_unlock (plugin->api_mutex);
			}

			g_mutex_unlock (plugin->write_mutex);

			ret = xmms_output_read (output, buffer, 4096);

			if (ret > 0) {
				xmms_error_t err;
				xmms_error_reset (&err);
				g_mutex_lock (plugin->api_mutex);
				plugin->methods.write (output, buffer, ret, &err);
				g_mutex_unlock (plugin->api_mutex);
				if (xmms_error_iserror (&err)) {
					XMMS_DBG ("Write method set error bit");
					plugin->write_status = XMMS_PLAYBACK_STATUS_STOP;
					xmms_output_set_error (output, &err);
				}
			}
			g_mutex_lock (plugin->write_mutex);
		}
	}

	g_assert (!output);

	g_mutex_unlock (plugin->write_mutex);

	XMMS_DBG ("Output driving thread exiting!");

	return NULL;
}
