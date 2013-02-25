/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <xmmspriv/xmms_outputplugin.h>
#include <xmmspriv/xmms_plugin.h>
#include <xmmspriv/xmms_thread_name.h>
#include <xmms/xmms_log.h>

struct xmms_output_plugin_St {
	xmms_plugin_t plugin;

	xmms_output_methods_t methods;

	/* make sure we only do one call at a time */
	GMutex *api_mutex;

	/* */
	xmms_playback_status_t wanted_status;
	gboolean write_running;
	GMutex *write_mutex;
	GCond *write_cond;
	GThread *write_thread;

	GCond *status_cond;
	GMutex *status_mutex;
	xmms_playback_status_t status;

	xmms_output_t *write_output;
};

static gboolean xmms_output_plugin_writer_status (xmms_output_plugin_t *plugin,
                                                  xmms_output_t *output,
                                                  xmms_playback_status_t s);
static void xmms_output_plugin_writer_status_wait (xmms_output_plugin_t *plugin,
                                                   xmms_output_t *output,
                                                   xmms_playback_status_t st);
static gpointer xmms_output_plugin_writer (gpointer data);


static void
xmms_output_plugin_destroy (xmms_object_t *obj)
{
	xmms_output_plugin_t *plugin = (xmms_output_plugin_t *)obj;

	g_mutex_free (plugin->api_mutex);
	g_mutex_free (plugin->write_mutex);
	g_cond_free (plugin->write_cond);

	g_cond_free (plugin->status_cond);
	g_mutex_free (plugin->status_mutex);

	xmms_plugin_destroy ((xmms_plugin_t *)obj);
}


xmms_plugin_t *
xmms_output_plugin_new (void)
{
	xmms_output_plugin_t *res;

	res = xmms_object_new (xmms_output_plugin_t, xmms_output_plugin_destroy);
	res->api_mutex = g_mutex_new ();
	res->write_mutex = g_mutex_new ();
	res->write_cond = g_cond_new ();

	res->status_cond = g_cond_new ();
	res->status_mutex = g_mutex_new ();

	return (xmms_plugin_t *)res;
}


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

	if (w == s) {
		XMMS_DBG ("Plugin needs to provide either write or status.");
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
		plugin->wanted_status = XMMS_PLAYBACK_STATUS_STOP;
		plugin->status = XMMS_PLAYBACK_STATUS_STOP;
	}

	return ret;
}


void
xmms_output_plugin_method_destroy (xmms_output_plugin_t *plugin,
                                   xmms_output_t *output)
{
	g_return_if_fail (output);
	g_return_if_fail (plugin);

	if (plugin->write_thread) {
		xmms_output_plugin_writer_status_wait (plugin, output,
		                                       XMMS_PLAYBACK_STATUS_STOP);

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


gboolean
xmms_output_plugin_format_set_always (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	if (plugin->methods.format_set_always) {
		return TRUE;
	}
	return FALSE;
}


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


gboolean
xmms_output_plugin_method_volume_set_available (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return !!plugin->methods.volume_set;
}


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


gboolean
xmms_output_plugin_method_volume_get_available (xmms_output_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return !!plugin->methods.volume_get;
}


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
	plugin->wanted_status = status;
	plugin->write_output = output;
	g_cond_signal (plugin->write_cond);
	g_mutex_unlock (plugin->write_mutex);

	return TRUE;
}

static void
xmms_output_plugin_writer_status_wait (xmms_output_plugin_t *plugin,
                                       xmms_output_t *output,
                                       xmms_playback_status_t status)
{
	g_mutex_lock (plugin->status_mutex);

	if (plugin->wanted_status != status) {
		xmms_output_plugin_writer_status (plugin, output, status);
	}

	while (plugin->status != status) {
		g_cond_wait (plugin->status_cond, plugin->status_mutex);
	}

	g_mutex_unlock (plugin->status_mutex);
}


static gpointer
xmms_output_plugin_writer (gpointer data)
{
	xmms_output_plugin_t *plugin = (xmms_output_plugin_t *) data;
	xmms_output_t *output = NULL;
	gchar buffer[4096];
	gint ret;

	xmms_set_thread_name ("x2 out writer");

	g_mutex_lock (plugin->write_mutex);

	while (plugin->write_running) {
		if (plugin->wanted_status == XMMS_PLAYBACK_STATUS_STOP) {
			if (output) {
				g_mutex_lock (plugin->api_mutex);
				plugin->methods.close (output);
				g_mutex_unlock (plugin->api_mutex);

				output = NULL;
			}

			g_mutex_lock (plugin->status_mutex);
			plugin->status = plugin->wanted_status;
			g_cond_signal (plugin->status_cond);
			g_mutex_unlock (plugin->status_mutex);


			g_cond_wait (plugin->write_cond, plugin->write_mutex);
		} else if (plugin->wanted_status == XMMS_PLAYBACK_STATUS_PAUSE) {
			xmms_config_property_t *p;

			p = xmms_config_lookup ("output.flush_on_pause");
			if (xmms_config_property_get_int (p)) {
				g_mutex_lock (plugin->api_mutex);
				plugin->methods.flush (output);
				g_mutex_unlock (plugin->api_mutex);
			}

			g_mutex_lock (plugin->status_mutex);
			plugin->status = plugin->wanted_status;
			g_cond_signal (plugin->status_cond);
			g_mutex_unlock (plugin->status_mutex);

			g_cond_wait (plugin->write_cond, plugin->write_mutex);
		} else if (plugin->wanted_status == XMMS_PLAYBACK_STATUS_PLAY) {
			if (!output) {
				gboolean ret;

				output = plugin->write_output;

				g_mutex_lock (plugin->api_mutex);
				ret = plugin->methods.open (output);
				g_mutex_unlock (plugin->api_mutex);

				if (!ret) {
					xmms_log_error ("Could not open output");
					plugin->wanted_status = XMMS_PLAYBACK_STATUS_STOP;
					output = NULL;
					continue;
				}
			}

			g_mutex_lock (plugin->status_mutex);
			plugin->status = plugin->wanted_status;
			g_cond_signal (plugin->status_cond);
			g_mutex_unlock (plugin->status_mutex);

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

					g_mutex_lock (plugin->write_mutex);
					plugin->wanted_status = XMMS_PLAYBACK_STATUS_STOP;
					g_mutex_unlock (plugin->write_mutex);

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
