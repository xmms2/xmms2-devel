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

#include "xmms/effect.h"
#include "xmms/plugin.h"
#include "xmms/object.h"
#include "xmms/config.h"
#include "xmms/util.h"

#include "internal/plugin_int.h"

/** @defgroup Effect Effect
  * @ingroup XMMSServer
  * @{
  */

struct xmms_effect_St {
	void (*deinit) (xmms_effect_t *);
	void (*samplerate_change) (xmms_effect_t *, guint rate);
	void (*run) (xmms_effect_t *, gchar *buf, guint len);

	gpointer *private_data;
	guint rate;
	xmms_plugin_t *plugin;

	/* configval for effect.foo.enabled */
	xmms_config_value_t *cfg_enabled;
	gboolean enabled;
};

void
xmms_effect_samplerate_set (xmms_effect_t *e, guint rate)
{
	if (rate != e->rate) {
		e->rate = rate;
		e->samplerate_change (e, rate);
	}
}

void
xmms_effect_run (xmms_effect_t *e, gchar *buf, guint len)
{
	if (e->enabled) {
		e->run (e, buf, len);
	}
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

xmms_plugin_t *
xmms_effect_plugin_get (xmms_effect_t *effect)
{
	g_return_val_if_fail (effect, NULL);

	return (effect->plugin);
}

/**
 *
 * @internal
 */

static void
on_enabled_changed (xmms_object_t *object, gconstpointer value,
                    gpointer udata)
{
	xmms_effect_t *effect = udata;

	effect->enabled = !strcmp (value, "1");
}

xmms_effect_t *
xmms_effect_new (xmms_plugin_t *plugin)
{
	void (*initfunc) (xmms_effect_t *);
	xmms_effect_t *effect;

	g_return_val_if_fail (plugin, NULL);

	effect = g_new0 (xmms_effect_t, 1);
	if (!effect)
		return NULL;

	effect->plugin = plugin;
	xmms_object_ref (effect->plugin);

	initfunc = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_INIT);
	if (!initfunc) {
		xmms_object_unref (effect->plugin);
		g_free (effect);

		return NULL;
	}

	initfunc (effect);

	effect->samplerate_change = xmms_plugin_method_get (plugin,
			XMMS_PLUGIN_METHOD_SAMPLERATE_SET);

	effect->run = xmms_plugin_method_get (plugin,
			XMMS_PLUGIN_METHOD_PROCESS);

	effect->deinit = xmms_plugin_method_get (plugin,
			XMMS_PLUGIN_METHOD_DEINIT);

	/* check whether this plugin is enabled.
	 * if the plugin doesn't provide the "enabled" config key,
	 * we'll just assume it cannot be disabled.
	 */
	effect->cfg_enabled = xmms_plugin_config_lookup (plugin, "enabled");

	if (!effect->cfg_enabled) {
		effect->enabled = TRUE;
	} else {
		effect->enabled = !!xmms_config_value_int_get (effect->cfg_enabled);

		xmms_config_value_callback_set (effect->cfg_enabled,
		                                on_enabled_changed, effect);
	}

	return effect;
}

void
xmms_effect_free (xmms_effect_t *effect)
{
	if (!effect)
		return;

	if (effect->deinit) {
		effect->deinit (effect);
	}

	xmms_object_unref (effect->plugin);

	if (effect->cfg_enabled) {
		xmms_config_value_callback_remove (effect->cfg_enabled,
		                                   on_enabled_changed);
	}

	g_free (effect);
}

/** @} */

