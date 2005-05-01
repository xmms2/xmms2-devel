/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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




/**
 * @file
 * 
 */


#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include "xmms/effect.h"
#include "xmms/plugin.h"
#include "xmms/object.h"
#include "xmms/config.h"
#include "xmms/util.h"

#include "internal/plugin_int.h"


static void on_enabled_changed (xmms_object_t *object, gconstpointer value, gpointer udata);

/** @defgroup Effect Effect
  * @ingroup XMMSServer
  * @brief Manipulates decoded data.
  * @{
  */

/** 
 * Effect structure
 */
struct xmms_effect_St {
	void (*destroy) (xmms_effect_t *);
	gboolean (*format_set) (xmms_effect_t *, xmms_audio_format_t *);
	void (*run) (xmms_effect_t *, gchar *, guint);
	void (*current_mlib_entry) (xmms_effect_t *, xmms_medialib_entry_t);

	gpointer *private_data;
	xmms_plugin_t *plugin;

	/* configval for effect.foo.enabled */
	xmms_config_value_t *cfg_enabled;
	gboolean enabled;
};

static void
on_currentid_changed (xmms_object_t *object,
                      const xmms_object_cmd_arg_t *arg,
                      xmms_effect_t *effect);

/**
 * Run the current effect on the the data you feed it.
 * 
 * @param e the effect to use
 * @param buf the buffer with unencoded data read from a decoder
 * @param len the length of #buf
 */

void
xmms_effect_run (xmms_effect_t *e, xmms_sample_t *buf, guint len)
{
	if (e->enabled) {
		e->run (e, buf, len);
	}
}

/**
 * Set the current stream format for the effect plugin
 */

gboolean
xmms_effect_format_set (xmms_effect_t *e, xmms_audio_format_t *fmt)
{
	return e->format_set (e, fmt);
}


/**
 * Retreive plugin-private data.
 *
 * @param effect 
 * @returns the data
 */
gpointer
xmms_effect_private_data_get (xmms_effect_t *effect)
{
	gpointer ret;
	g_return_val_if_fail (effect, NULL);

	ret = effect->private_data;

	return ret;
}

/**
 * Set plugin-private data.
 *
 * @param effect
 * @param data
 */
void
xmms_effect_private_data_set (xmms_effect_t *effect, gpointer data)
{
	g_return_if_fail (effect);

	effect->private_data = data;
}

/**
 * Return the plugin that is used by this effect 
 */
xmms_plugin_t *
xmms_effect_plugin_get (xmms_effect_t *effect)
{
	g_return_val_if_fail (effect, NULL);

	return (effect->plugin);
}

/**
 * Allocate a new effect for the plugin
 */

xmms_effect_t *
xmms_effect_new (xmms_plugin_t *plugin, xmms_output_t *output)
{
	void (*newfunc) (xmms_effect_t *, xmms_output_t *);
	xmms_effect_t *effect;

	g_return_val_if_fail (plugin, NULL);

	effect = g_new0 (xmms_effect_t, 1);
	if (!effect)
		return NULL;

	effect->plugin = plugin;
	xmms_object_ref (effect->plugin);

	newfunc = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);
	if (!newfunc) {
		xmms_object_unref (effect->plugin);
		g_free (effect);

		return NULL;
	}

	newfunc (effect, output);

	effect->format_set = xmms_plugin_method_get (plugin,
	                                             XMMS_PLUGIN_METHOD_FORMAT_SET);

	effect->run = xmms_plugin_method_get (plugin,
	                                      XMMS_PLUGIN_METHOD_PROCESS);

	effect->current_mlib_entry =
		xmms_plugin_method_get (plugin,
		                        XMMS_PLUGIN_METHOD_CURRENT_MEDIALIB_ENTRY);

	effect->destroy = xmms_plugin_method_get (plugin,
	                                          XMMS_PLUGIN_METHOD_DESTROY);

	/* check whether this plugin is enabled. */
	effect->cfg_enabled =
		xmms_plugin_config_value_register (plugin, "enabled", "0",
		                                   on_enabled_changed, effect);
	effect->enabled = !!xmms_config_value_int_get (effect->cfg_enabled);

	xmms_object_connect (XMMS_OBJECT (output),
	                     XMMS_IPC_SIGNAL_OUTPUT_CURRENTID,
	                     (xmms_object_handler_t ) on_currentid_changed,
	                     effect);

	return effect;
}


/**
 * Free all resources used by the effect
 */

void
xmms_effect_free (xmms_effect_t *effect)
{
	if (!effect)
		return;

	if (effect->destroy) {
		effect->destroy (effect);
	}

	xmms_object_unref (effect->plugin);

	xmms_config_value_callback_remove (effect->cfg_enabled,
	                                   on_enabled_changed);

	g_free (effect);
}

static void
on_currentid_changed (xmms_object_t *object,
                      const xmms_object_cmd_arg_t *arg,
                      xmms_effect_t *effect)
{
	effect->current_mlib_entry (effect,
		(xmms_medialib_entry_t) arg->retval.uint32);
}

/** @} */

/**
 * @internal
 */

static void
on_enabled_changed (xmms_object_t *object, gconstpointer value,
                    gpointer udata)
{
	xmms_effect_t *effect = udata;

	effect->enabled = value ? !!atoi (value) : FALSE;
}


