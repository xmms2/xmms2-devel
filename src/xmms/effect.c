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
	struct xmms_effect_St *next;
	void (*deinit) (xmms_effect_t *);
	void (*samplerate_change) (xmms_effect_t *, guint rate);
	void (*run) (xmms_effect_t *, gchar *buf, guint len);
	gpointer *private_data;
	guint rate;
	xmms_plugin_t *plugin;
};

void
xmms_effect_samplerate_set (xmms_effect_t *effects, guint rate)
{

	for (; effects; effects = effects->next) {
		if (rate != effects->rate) {
			effects->rate = rate;
			effects->samplerate_change (effects, rate);
		}
	}
}

void
xmms_effect_run (xmms_effect_t *effects, gchar *buf, guint len)
{
	for (; effects; effects = effects->next) {
		effects->run (effects, buf, len);
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

xmms_effect_t *
xmms_effect_prepend (xmms_effect_t *stack, gchar *name)
{
	GList *list, *l;
	xmms_plugin_t *plugin = NULL;
	void (*initfunc) (xmms_effect_t *);
	xmms_effect_t *effect;

	g_return_val_if_fail (name, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_EFFECT);

	for (l = list; l; l = g_list_next (l)) {
		if (!g_strcasecmp (xmms_plugin_shortname_get (l->data),
		                   name)) {
			plugin = l->data;
			xmms_plugin_ref (plugin);
			break;
		}
	}

	xmms_plugin_list_destroy (list);

	if (!plugin) {
		XMMS_DBG ("Skipping unknown plugin: %s", name);
		return stack;
	}

	effect = g_new0 (xmms_effect_t, 1);

	/* if we exit early here, make sure we also unref
	 * the plugin again
	 */
	if (!effect) {
		xmms_plugin_unref (plugin);
	}
	g_return_val_if_fail (effect, stack);

	effect->plugin = plugin;

	initfunc = xmms_plugin_method_get (plugin, 
			XMMS_PLUGIN_METHOD_INIT);

	/* see above */
	if (!initfunc) {
		xmms_plugin_unref (plugin);
	}
	g_return_val_if_fail (initfunc, stack);

	initfunc (effect);

	effect->samplerate_change = xmms_plugin_method_get (plugin, 
			XMMS_PLUGIN_METHOD_SAMPLERATE_SET);

	effect->run = xmms_plugin_method_get (plugin, 
			XMMS_PLUGIN_METHOD_PROCESS);

	effect->deinit = xmms_plugin_method_get (plugin, 
			XMMS_PLUGIN_METHOD_DEINIT);

	effect->next = stack;
	stack = effect;

	return stack;
}

/** @} */

