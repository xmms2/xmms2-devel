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

#include "xmms/plugin.h"
#include "xmms/util.h"
#include "xmms/config.h"
#include "xmms/object.h"

#include "internal/plugin_int.h"

#include <gmodule.h>
#include <string.h>

#ifdef XMMS_OS_DARWIN
#define XMMS_LIBSUFFIX ".dylib"
#else
#define XMMS_LIBSUFFIX ".so"
#endif


extern xmms_config_t *global_config;

struct xmms_plugin_St {
	GMutex *mutex;
	GModule *module;
	GList *info_list;

	xmms_plugin_type_t type;
	gchar *name;
	gchar *shortname;
	gchar *description;
	gint properties;

	guint users;
	GHashTable *method_table;
};

/*
 * Global variables
 */

static GMutex *xmms_plugin_mtx;
static GList *xmms_plugin_list;

/*
 * Function prototypes
 */

static gchar *plugin_config_path (xmms_plugin_t *plugin, const gchar *value);

/*
 * Public functions
 */

/**
 * @defgroup XMMSPLugin XMMSPlugin
 * @brief All functions relevant to a plugin.
 *
 * Every plugin has a initfunction. That should be defined like following:
 * @code
 * xmms_plugin_t *xmms_plugin_get (void)
 * @endcode
 * 
 * This function must call xmms_plugin_new() with the appropiate
 * arguments. 
 * This function can also call xmms_plugin_info_add(), xmms_plugin_method_add(),
 * xmms_plugin_properties_add()
 *
 * A example plugin here is:
 * @code
 * xmms_plugin_t *
 * xmms_plugin_get (void) {
 * 	xmms_plugin_t *plugin;
 *	
 *	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_EXAMPLE, "test",
 *				  "Test plugin" XMMS_VERSION,
 *				  "A very simple plugin");
 *	xmms_plugin_info_add (plugin, "Author", "Karsten Brinkmann");
 *	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_TEST, xmms_test);
 *	return plugin;
 * }
 * @endcode
 *
 * @{
 */

/**
 * The following function creates a new plugin.
 *
 * @param type The type of plugin. Each type is
 * explained in the chapters about that plugintype.
 * @param shortname The short version of pluginname. Eg. "oss"
 * @param name The fullname of the plugin. Eg. "Open Sound System"
 * @param description A descriptive text about the plugin.
 *
 * @return a new plugin of the given type.
 */
xmms_plugin_t *
xmms_plugin_new (xmms_plugin_type_t type, const gchar *shortname,
				const gchar *name,
				const gchar *description)
{
	xmms_plugin_t *plugin;

	g_return_val_if_fail (name, NULL);
	g_return_val_if_fail (description, NULL);
	
	plugin = g_new0 (xmms_plugin_t, 1);

	plugin->mutex = g_mutex_new ();
	plugin->type = type;
	plugin->name = g_strdup (name);
	plugin->shortname = g_strdup (shortname);
	plugin->description = g_strdup (description);
	plugin->method_table = g_hash_table_new (g_str_hash, g_str_equal);
	plugin->info_list = g_list_alloc ();

	return plugin;
}

/**
 * Adds a method to this plugin. Each diffrent type must define
 * diffrent type of methods.
 *
 * @param plugin A new plugin created from xmms_plugin_new()
 * @param name The name of the name that method to add. The names are
 * defined and should not just be a string. Se each plugintype for
 * documentation.
 * @param method The function pointer to the method.
 */

void
xmms_plugin_method_add (xmms_plugin_t *plugin, const gchar *name,
			xmms_plugin_method_t method)
{
	g_return_if_fail (plugin);
	g_return_if_fail (name);
	g_return_if_fail (method);

	g_mutex_lock (plugin->mutex);
	g_hash_table_insert (plugin->method_table, g_strdup (name), method);
	g_mutex_unlock (plugin->mutex);
}

/**
 * Set a property for this plugin.
 * The diffrent properties are defined under each
 * plugintypes documentation.
 */

void
xmms_plugin_properties_add (xmms_plugin_t* const plugin, gint property)
{
	g_return_if_fail (plugin);
	g_return_if_fail (property);

	plugin->properties |= property;

}

/**
 * Remove property for this plugin.
 */

void
xmms_plugin_properties_remove (xmms_plugin_t* const plugin, gint property)
{
	g_return_if_fail (plugin);
	g_return_if_fail (property);

	plugin->properties &= ~property;

}

/**
 * Add information to the plugin. This information can be
 * viewed in a client. The information can be for example
 * the name of the author or the webpage of the plugin.
 *
 * @param plugin The plugin to set the info in.
 * @param key This can be any given string.
 * @param value Value of this key.
 */

void
xmms_plugin_info_add (xmms_plugin_t *plugin, gchar *key, gchar *value)
{
	xmms_plugin_info_t *info;
	g_return_if_fail (plugin);

	info = g_new0 (xmms_plugin_info_t, 1);
	info->key = key;
	info->value = value;

	g_list_append (plugin->info_list, info);
}

/* @} */

gboolean
xmms_plugin_properties_check (const xmms_plugin_t *plugin, gint property)
{
	g_return_val_if_fail (plugin, FALSE);
	g_return_val_if_fail (property, FALSE);

	return plugin->properties & property;

}

xmms_plugin_type_t
xmms_plugin_type_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, 0);
	
	return plugin->type;
}

const char *
xmms_plugin_name_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->name;
}

const gchar *
xmms_plugin_shortname_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->shortname;
}

const char *
xmms_plugin_description_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->description;
}

const GList*
xmms_plugin_info_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->info_list;
}

/*
 * Private functions
 */

gboolean
xmms_plugin_init (gchar *path)
{
	xmms_plugin_mtx = g_mutex_new ();

	if (!path)
		path = PKGLIBDIR;

	xmms_plugin_scan_directory (path);
	
	return TRUE;
}

void
xmms_plugin_scan_directory (const gchar *dir)
{
	GDir *d;
	const char *name;
	gchar *path;
	GModule *module;
	xmms_plugin_t *(*plugin_init) (void);
	xmms_plugin_t *plugin;
	gpointer sym;

	g_return_if_fail (global_config);

	XMMS_DBG ("Scanning directory: %s", dir);
	
	d = g_dir_open (dir, 0, NULL);
	if (!d) {
		xmms_log_error ("Failed to open directory");
		return;
	}

	g_mutex_lock (xmms_plugin_mtx);
	while ((name = g_dir_read_name (d))) {
		if (strncmp (name, "lib", 3) != 0)
			continue;

		if (!strstr (name, XMMS_LIBSUFFIX))
			continue;

		path = g_build_filename (dir, name, NULL);
		if (!g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			g_free (path);
			continue;
		}

		XMMS_DBG ("Trying to load file: %s", path);
		module = g_module_open (path, 0);
		if (!module) {
			XMMS_DBG ("%s", g_module_error ());
			g_free (path);
			continue;
		}

		if (!g_module_symbol (module, "xmms_plugin_get", &sym)) {
			g_module_close (module);
			g_free (path);
			continue;
		}

		plugin_init = sym;

		plugin = plugin_init ();
		if (plugin) {
			const GList *info;
			const xmms_plugin_info_t *i;

			XMMS_DBG ("Adding plugin: %s", plugin->name);
			info = xmms_plugin_info_get (plugin);
			while (info) {
				i = info->data;
				if (i)
					XMMS_DBG ("INFO: %s = %s", i->key,i->value);
				info = g_list_next (info);
			}
			plugin->module = module;
			xmms_plugin_list = g_list_prepend (xmms_plugin_list, plugin);
		} else {
			g_module_close (module);
		}

		g_free (path);
	}
	g_mutex_unlock (xmms_plugin_mtx);
	g_dir_close (d);
}

void
xmms_plugin_ref (xmms_plugin_t *plugin)
{
	g_return_if_fail (plugin);

	g_mutex_lock (plugin->mutex);
	plugin->users++;
	g_mutex_unlock (plugin->mutex);
}

void
xmms_plugin_unref (xmms_plugin_t *plugin)
{
	g_return_if_fail (plugin);

	g_mutex_lock (plugin->mutex);
	if (plugin->users > 0) {
		plugin->users--;
	} else {
		g_warning ("Tried to unref plugin %s with 0 users", plugin->name);
	}
	g_mutex_unlock (plugin->mutex);
}

GList *
xmms_plugin_list_get (xmms_plugin_type_t type)
{
	GList *list = NULL, *node;

	g_mutex_lock (xmms_plugin_mtx);

	for (node = xmms_plugin_list; node; node = g_list_next (node)) {
		xmms_plugin_t *plugin = node->data;

		if (plugin->type == type) {
			xmms_plugin_ref (plugin);
			list = g_list_prepend (list, plugin);
		}
	}
	
	g_mutex_unlock (xmms_plugin_mtx);
	
	return list;
}

void
xmms_plugin_list_destroy (GList *list)
{
	GList *node;

	if (!list)
		return;
	
	for (node = list; node; node = g_list_next (node))
		xmms_plugin_unref ((xmms_plugin_t *)node->data);

	g_list_free (list);
}

xmms_plugin_method_t
xmms_plugin_method_get (xmms_plugin_t *plugin, const gchar *method)
{
	xmms_plugin_method_t ret;

	g_return_val_if_fail (plugin, NULL);
	g_return_val_if_fail (method, NULL);

	g_mutex_lock (plugin->mutex);
	ret = g_hash_table_lookup (plugin->method_table, method);
	g_mutex_unlock (plugin->mutex);

	return ret;
}

xmms_config_value_t *
xmms_plugin_config_lookup (xmms_plugin_t *plugin,
			   const gchar *value)
{
	gchar *ppath;
	xmms_config_value_t *val;

	g_return_val_if_fail (plugin, NULL);
	g_return_val_if_fail (value, NULL);
	
	ppath = plugin_config_path (plugin, value);
	val = xmms_config_lookup (ppath);

	g_free (ppath);

	return val;
}

xmms_config_value_t *
xmms_plugin_config_value_register (xmms_plugin_t *plugin,
				   const gchar *value,
				   const gchar *default_value,
				   xmms_object_handler_t cb,
				   gpointer userdata)
{
	const gchar *fullpath;
	xmms_config_value_t *val;

	g_return_val_if_fail (plugin, NULL);
	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (default_value, NULL);

	fullpath = plugin_config_path (plugin, value);

	val = xmms_config_value_register (fullpath, 
					  default_value, 
					  cb, userdata);
	return val;
}

/*
 * Static functions
 */

static gchar *
plugin_config_path (xmms_plugin_t *plugin, const gchar *value)
{
	gchar *ret;
	gchar *pl;

	switch (xmms_plugin_type_get (plugin)) {
		case XMMS_PLUGIN_TYPE_TRANSPORT:
			pl = "transport";
			break;
		case XMMS_PLUGIN_TYPE_DECODER:
			pl = "decoder";
			break;
		case XMMS_PLUGIN_TYPE_OUTPUT:
			pl = "output";
			break;
		case XMMS_PLUGIN_TYPE_MEDIALIB:
			pl = "medialib";
			break;
		case XMMS_PLUGIN_TYPE_PLAYLIST:
			pl = "playlist";
			break;
		case XMMS_PLUGIN_TYPE_EFFECT:
			pl = "effect";
			break;
		default:
			pl = "noname";
			break;
	}

	ret = g_strdup_printf ("%s.%s.%s", pl, xmms_plugin_shortname_get (plugin), value);

	return ret;
}

