/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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

#include <xmmspriv/xmms_xform_object.h>
#include <xmmspriv/xmms_xform.h>
#include <xmms/xmms_config.h>
#include <xmms/xmms_ipc.h>
#include <xmms/xmms_log.h>

struct xmms_xform_object_St {
	xmms_object_t obj;
};

static xmmsv_t *xmms_xform_client_browse (xmms_xform_object_t *obj, const gchar *url, xmms_error_t *error);
static void xmms_xform_object_destroy (xmms_object_t *obj);
static void xmms_xform_effect_callbacks_init (void);
static void xmms_xform_effect_properties_update (xmms_object_t *object, xmmsv_t *data, gpointer udata);

#include "xform_ipc.c"

xmms_xform_object_t *
xmms_xform_object_init ()
{
	xmms_xform_object_t *obj;

	obj = xmms_object_new (xmms_xform_object_t, xmms_xform_object_destroy);

	xmms_xform_register_ipc_commands (XMMS_OBJECT (obj));

	xmms_xform_effect_callbacks_init ();

	return obj;
}

static void
xmms_xform_object_destroy (xmms_object_t *obj)
{
	XMMS_DBG ("Deactivating xform object");
	xmms_xform_unregister_ipc_commands ();
}

static xmmsv_t *
xmms_xform_client_browse (xmms_xform_object_t *obj, const gchar *url,
                          xmms_error_t *error)
{
	return xmms_xform_browse (url, error);
}

static void
xmms_xform_effect_callbacks_init (void)
{
	xmms_config_property_t *cfg;
	xmms_xform_plugin_t *plugin;
	gchar key[64];
	const gchar *name;
	gint effect_no;

	for (effect_no = 0; ; effect_no++) {
		g_snprintf (key, sizeof (key), "effect.order.%i", effect_no);
		cfg = xmms_config_lookup (key);
		if (!cfg) {
			break;
		}

		xmms_config_property_callback_set (cfg, xmms_xform_effect_properties_update,
		                                   GINT_TO_POINTER (effect_no));

		name = xmms_config_property_get_string (cfg);
		if (!name[0]) {
			continue;
		}

		plugin = xmms_xform_find_plugin (name);
		if (!plugin) {
			xmms_log_error ("Couldn't find any effect named '%s'", name);
			continue;
		}

		xmms_xform_plugin_config_property_register (plugin, "enabled",
		                                            "1", NULL, NULL);

		xmms_object_unref (plugin);
	}

	/* the name stored in the last present property was not "" or there was no
	   last present property */
	if ((!effect_no) || name[0]) {
		xmms_config_property_register (key, "", xmms_xform_effect_properties_update,
		                               GINT_TO_POINTER (effect_no));
	}
}

static void
xmms_xform_effect_properties_update (xmms_object_t *object, xmmsv_t *data, gpointer udata)
{
	xmms_config_property_t *cfg = (xmms_config_property_t *) object;
	xmms_xform_plugin_t *plugin;
	const gchar *name;
	gchar key[64];
	gint effect_no = GPOINTER_TO_INT (udata);

	name = xmms_config_property_get_string (cfg);
	if (!name[0]) {
		return;
	}

	plugin = xmms_xform_find_plugin (name);
	if (!plugin) {
		xmms_log_error ("Couldn't find any effect named '%s'", name);
	} else {
		xmms_xform_plugin_config_property_register (plugin, "enabled",
		                                            "1", NULL, NULL);
		xmms_object_unref (plugin);
	}

	/* setup new effect.order.n */
	g_snprintf (key, sizeof (key), "effect.order.%i", effect_no + 1);
	if (!xmms_config_lookup (key)) {
		xmms_config_property_register (key, "", xmms_xform_effect_properties_update,
		                               GINT_TO_POINTER (effect_no + 1));
	}
}
