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

#include "xmmspriv/xmms_effect.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_medialib.h"

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
	xmms_config_property_t *cfg_enabled;
	gboolean enabled;
};

static void
on_enabled_changed (xmms_object_t *object, gconstpointer value,
                    gpointer udata);


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
	if (e->format_set) {
		return e->format_set (e, fmt);
	} else {
		return TRUE;
	}
}

void
xmms_effect_entry_set (xmms_effect_t *effect, xmms_medialib_entry_t entry)
{
	if (effect->current_mlib_entry)
		effect->current_mlib_entry (effect, entry);
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
xmms_effect_new (xmms_plugin_t *plugin)
{
	void (*newfunc) (xmms_effect_t *);
	xmms_effect_t *effect;

	g_return_val_if_fail (plugin, NULL);

	effect = g_new0 (xmms_effect_t, 1);
	if (!effect)
		return NULL;

	effect->plugin = plugin;
	xmms_object_ref (effect->plugin);

	newfunc = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_NEW);
	g_assert (newfunc);

	newfunc (effect);

	effect->format_set = xmms_plugin_method_get (plugin,
	                                             XMMS_PLUGIN_METHOD_FORMAT_SET);

	effect->run = xmms_plugin_method_get (plugin,
	                                      XMMS_PLUGIN_METHOD_PROCESS);

	effect->current_mlib_entry =
		xmms_plugin_method_get (plugin,
		                        XMMS_PLUGIN_METHOD_CURRENT_MEDIALIB_ENTRY);

	effect->destroy = xmms_plugin_method_get (plugin,
	                                          XMMS_PLUGIN_METHOD_DESTROY);
	g_assert (effect->destroy);

	/* check whether this plugin is enabled. */
	effect->cfg_enabled = xmms_plugin_config_lookup (plugin, "enabled");
	g_assert (effect->cfg_enabled);

	xmms_config_property_callback_set (effect->cfg_enabled,
	                                   on_enabled_changed, effect);
	effect->enabled = !!xmms_config_property_get_int (effect->cfg_enabled);

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

	effect->destroy (effect);

	xmms_object_unref (effect->plugin);

	xmms_config_property_callback_remove (effect->cfg_enabled,
	                                   on_enabled_changed);

	g_free (effect);
}

gboolean
xmms_effect_plugin_verify (xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return xmms_plugin_has_methods (plugin,
	                                XMMS_PLUGIN_METHOD_NEW,
	                                XMMS_PLUGIN_METHOD_DESTROY,
	                                XMMS_PLUGIN_METHOD_PROCESS,
	                                NULL);
}

gboolean
xmms_effect_plugin_register (xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, FALSE);

	return !!xmms_plugin_config_property_register (plugin,
	                                               "enabled", "0",
	                                               NULL, NULL);
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


