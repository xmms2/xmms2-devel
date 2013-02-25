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




#ifndef __XMMS_PLUGIN_INT_H__
#define __XMMS_PLUGIN_INT_H__

#include <xmms/xmms_object.h>
#include <xmms/xmms_plugin.h>
#include <xmms/xmms_config.h>

#include <gmodule.h>

typedef struct xmms_plugin_St {
	xmms_object_t object;
	GModule *module;

	xmms_plugin_type_t type;
	const gchar *name;
	const gchar *shortname;
	const gchar *description;
	const gchar *version;
} xmms_plugin_t;

/*
 * Private functions
 */
gboolean xmms_plugin_load (const xmms_plugin_desc_t *desc, GModule *module);
gboolean xmms_plugin_init (const gchar *path);
void xmms_plugin_shutdown (void);
void xmms_plugin_destroy (xmms_plugin_t *plugin);

typedef gboolean (*xmms_plugin_foreach_func_t)(xmms_plugin_t *, gpointer);
void xmms_plugin_foreach (xmms_plugin_type_t type, xmms_plugin_foreach_func_t func, gpointer user_data);

xmms_plugin_t *xmms_plugin_find (xmms_plugin_type_t type, const gchar *name);

xmms_plugin_type_t xmms_plugin_type_get (const xmms_plugin_t *plugin);
const char *xmms_plugin_name_get (const xmms_plugin_t *plugin);
const gchar *xmms_plugin_shortname_get (const xmms_plugin_t *plugin);
const gchar *xmms_plugin_version_get (const xmms_plugin_t *plugin);
const char *xmms_plugin_description_get (const xmms_plugin_t *plugin);

xmms_config_property_t *xmms_plugin_config_lookup (xmms_plugin_t *plugin, const gchar *key);
xmms_config_property_t *xmms_plugin_config_property_register (xmms_plugin_t *plugin, const gchar *name, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);


#define XMMS_BUILTIN(type, api_ver, shname, name, ver, desc, setupfunc)	\
	const xmms_plugin_desc_t xmms_builtin_##shname = {			\
		type,							\
		api_ver,						\
		G_STRINGIFY(shname),					\
		name,							\
		ver,							\
		desc,							\
		setupfunc						\
	};

#endif
