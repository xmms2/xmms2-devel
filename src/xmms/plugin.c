/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include "xmms_configuration.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmms/xmms_config.h"
#include "xmmspriv/xmms_config.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_log.h"
#include "xmmspriv/xmms_playlist.h"
#include "xmmspriv/xmms_outputplugin.h"
#include "xmmspriv/xmms_xform.h"

#include <gmodule.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_VALGRIND
# include <memcheck.h>
#endif

/* OSX uses the .bundle extension, but g_module_build_path returns .so. */
#ifdef USE_BUNDLES
#define get_module_ext(dir) g_build_filename (dir, "*.bundle", NULL)
#else
#define get_module_ext(dir) g_module_build_path (dir, "*")
#endif


/*
 * Global variables
 */
static GList *xmms_plugin_list;

/*
 * Function prototypes
 */
static gboolean xmms_plugin_setup (xmms_plugin_t *plugin, const xmms_plugin_desc_t *desc);
static gboolean xmms_plugin_load (const xmms_plugin_desc_t *desc, GModule *module);
static gboolean xmms_plugin_scan_directory (const gchar *dir);

/*
 * Public functions
 */


/**
 * @if internal
 * -- internal documentation section --
 * @addtogroup XMMSPlugin
 * @{
 */

/**
 * @internal 
 * Lookup the value of a plugin's config property, given the property key.
 * @param[in] plugin The plugin
 * @param[in] key The property key (config path)
 * @return A config value
 * @todo config value <-> property fixup
 */
xmms_config_property_t *
xmms_plugin_config_lookup (xmms_plugin_t *plugin,
                           const gchar *key)
{
	gchar path[XMMS_PLUGIN_SHORTNAME_MAX_LEN + 256];
	xmms_config_property_t *prop;

	g_return_val_if_fail (plugin, NULL);
	g_return_val_if_fail (key, NULL);

	g_snprintf (path, sizeof (path), "%s.%s",
	            xmms_plugin_shortname_get (plugin), key);
	prop = xmms_config_lookup (path);

	return prop;
}

/**
 * @internal 
 * Register a config property for a plugin.
 * @param[in] plugin The plugin
 * @param[in] name The property name
 * @param[in] default_value The default value for the property
 * @param[in] cb A callback function to be executed when the property value
 * changes
 * @param[in] userdata Pointer to data to be passed to the callback
 * @todo config value <-> property fixup
 */
xmms_config_property_t *
xmms_plugin_config_property_register (xmms_plugin_t *plugin,
                                      const gchar *name,
                                      const gchar *default_value,
                                      xmms_object_handler_t cb,
                                      gpointer userdata)
{
	gchar fullpath[XMMS_PLUGIN_SHORTNAME_MAX_LEN + 256];
	xmms_config_property_t *prop;

	g_return_val_if_fail (plugin, NULL);
	g_return_val_if_fail (name, NULL);
	g_return_val_if_fail (default_value, NULL);

	g_snprintf (fullpath, sizeof (fullpath), "%s.%s",
	            xmms_plugin_shortname_get (plugin), name);

	prop = xmms_config_property_register (fullpath, default_value, cb,
	                                      userdata);

	return prop;
}

/**
 * @internal Get the type of this plugin
 * @param[in] plugin The plugin
 * @return The plugin type (#xmms_plugin_type_t)
 */
xmms_plugin_type_t
xmms_plugin_type_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, 0);

	return plugin->type;
}

/**
 * @internal Get the plugin's name. This is just an accessor method.
 * @param[in] plugin The plugin
 * @return A string containing the plugin's name
 */
const char *
xmms_plugin_name_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->name;
}

/**
 * @internal Get the plugin's short name. This is just an accessor method.
 * @param[in] plugin The plugin
 * @return A string containing the plugin's short name
 */
const gchar *
xmms_plugin_shortname_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->shortname;
}

/**
 * @internal Get the plugin's version. This is just an accessor method.
 * @param[in] plugin The plugin
 * @return A string containing the plugin's version
 */
const gchar *
xmms_plugin_version_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->version;
}

/**
 * @internal Get the plugin's description. This is just an accessor method.
 * @param[in] plugin The plugin
 * @return A string containing the plugin's description
 */
const char *
xmms_plugin_description_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->description;
}

/*
 * Private functions
 */


static void
xmms_plugin_add_builtin_plugins (void)
{
	extern const xmms_plugin_desc_t xmms_builtin_ringbuf;
	extern const xmms_plugin_desc_t xmms_builtin_magic;
	extern const xmms_plugin_desc_t xmms_builtin_converter;
	extern const xmms_plugin_desc_t xmms_builtin_segment;
	extern const xmms_plugin_desc_t xmms_builtin_visualization;

	xmms_plugin_load (&xmms_builtin_ringbuf, NULL);
	xmms_plugin_load (&xmms_builtin_magic, NULL);
	xmms_plugin_load (&xmms_builtin_converter, NULL);
	xmms_plugin_load (&xmms_builtin_segment, NULL);
	xmms_plugin_load (&xmms_builtin_visualization, NULL);
}


/**
 * @internal Initialise the plugin system
 * @param[in] path Absolute path to the plugins directory.
 * @return Whether the initialisation was successful or not.
 */
gboolean
xmms_plugin_init (const gchar *path)
{
	if (!path)
		path = PKGLIBDIR;

	xmms_plugin_scan_directory (path);

	xmms_plugin_add_builtin_plugins ();
	return TRUE;
}

/**
 * @internal Shut down the plugin system. This function unrefs all the plugins
 * loaded.
 */
void
xmms_plugin_shutdown ()
{
#ifdef HAVE_VALGRIND
	/* print out a leak summary at this point, because the final leak
	 * summary won't include proper backtraces of leaks found in
	 * plugins, since we close the so's here.
	 *
	 * note: the following call doesn't do anything if we're not run
	 * in valgrind
	 */
	VALGRIND_DO_LEAK_CHECK
		;
#endif

	while (xmms_plugin_list) {
		xmms_plugin_t *p = xmms_plugin_list->data;

		/* if this plugin's refcount is > 1, then there's a bug
		 * in one of the other subsystems
		 */
		if (p->object.ref > 1) {
			XMMS_DBG ("%s's refcount is %i",
			          p->name, p->object.ref);
		}

		xmms_object_unref (p);

		xmms_plugin_list = g_list_delete_link (xmms_plugin_list,
		                                       xmms_plugin_list);
	}
}


static gboolean
xmms_plugin_load (const xmms_plugin_desc_t *desc, GModule *module)
{
	xmms_plugin_t *plugin;
	xmms_plugin_t *(*allocer) (void);
	gboolean (*verifier) (xmms_plugin_t *);
	gint expected_ver;

	switch (desc->type) {
	case XMMS_PLUGIN_TYPE_OUTPUT:
		expected_ver = XMMS_OUTPUT_API_VERSION;
		allocer = xmms_output_plugin_new;
		verifier = xmms_output_plugin_verify;
		break;
	case XMMS_PLUGIN_TYPE_XFORM:
		expected_ver = XMMS_XFORM_API_VERSION;
		allocer = xmms_xform_plugin_new;
		verifier = xmms_xform_plugin_verify;
		break;
	default:
		XMMS_DBG ("Unknown plugin type!");
		return FALSE;
	}

	if (desc->api_version != expected_ver) {
		XMMS_DBG ("Bad api version!");
		return FALSE;
	}

	plugin = allocer ();
	if (!plugin) {
		XMMS_DBG ("Alloc failed!");
		return FALSE;
	}

	if (!xmms_plugin_setup (plugin, desc)) {
		xmms_log_error ("Setup failed!");
		xmms_object_unref (plugin);
		return FALSE;
	}

	if (!desc->setup_func (plugin)) {
		xmms_log_error ("Setup function returned error!");
		xmms_object_unref (plugin);
		return FALSE;
	}

	if (!verifier (plugin)) {
		xmms_log_error ("Verify failed for plugin!");
		xmms_object_unref (plugin);
		return FALSE;
	}

	plugin->module = module;

	xmms_plugin_list = g_list_prepend (xmms_plugin_list, plugin);
	return TRUE;
}

/**
 * @internal Scan a particular directory for plugins to load
 * @param[in] dir Absolute path to plugins directory
 * @return TRUE if directory successfully scanned for plugins
 */
static gboolean
xmms_plugin_scan_directory (const gchar *dir)
{
	GDir *d;
	const char *name;
	gchar *path;
	gchar *temp;
	gchar *pattern;
	GModule *module;
	gpointer sym;

	temp = get_module_ext (dir);

	XMMS_DBG ("Scanning directory for plugins (%s)", temp);

	pattern = g_path_get_basename (temp);

	g_free (temp);

	d = g_dir_open (dir, 0, NULL);
	if (!d) {
		xmms_log_error ("Failed to open plugin directory (%s)", dir);
		return FALSE;
	}

	while ((name = g_dir_read_name (d))) {

		if (!g_pattern_match_simple (pattern, name))
			continue;

		path = g_build_filename (dir, name, NULL);
		if (!g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			g_free (path);
			continue;
		}

		XMMS_DBG ("Trying to load file: %s", path);
		module = g_module_open (path, 0);
		if (!module) {
			xmms_log_error ("Failed to open plugin %s: %s",
			                path, g_module_error ());
			g_free (path);
			continue;
		}

		if (!g_module_symbol (module, "XMMS_PLUGIN_DESC", &sym)) {
			xmms_log_error ("Failed to find plugin header in %s", path);
			g_module_close (module);
			g_free (path);
			continue;
		}
		g_free (path);

		if (!xmms_plugin_load ((const xmms_plugin_desc_t *) sym, module)) {
			g_module_close (module);
		}
	}

	g_dir_close (d);
	g_free (pattern);

	return TRUE;
}

/**
 * @internal Apply a function to all plugins of specified type.
 * @param[in] type The type of plugin to look for.
 * @param[in] func function to apply.
 * @param[in] user_data Userspecified data passed to function.
 */
void
xmms_plugin_foreach (xmms_plugin_type_t type, xmms_plugin_foreach_func_t func, gpointer user_data)
{
	GList *node;

	for (node = xmms_plugin_list; node; node = g_list_next (node)) {
		xmms_plugin_t *plugin = node->data;

		if (plugin->type == type || type == XMMS_PLUGIN_TYPE_ALL) {
			if (!func (plugin, user_data))
				break;
		}
	}
}

typedef struct {
	const gchar *name;
	xmms_plugin_t *plugin;
} xmms_plugin_find_foreach_data_t;

static gboolean
xmms_plugin_find_foreach (xmms_plugin_t *plugin, gpointer udata)
{
	xmms_plugin_find_foreach_data_t *data = udata;

	if (!g_ascii_strcasecmp (plugin->shortname, data->name)) {
		xmms_object_ref (plugin);
		data->plugin = plugin;
		return FALSE;
	}
	return TRUE;
}

/**
 * @internal Find a plugin that's been loaded, by a particular type and name
 * @param[in] type The type of plugin to look for
 * @param[in] name The name of the plugin to look for
 * @return The plugin instance, if found. NULL otherwise.
 */
xmms_plugin_t *
xmms_plugin_find (xmms_plugin_type_t type, const gchar *name)
{
	xmms_plugin_find_foreach_data_t data = {name, NULL};
	xmms_plugin_foreach (type, xmms_plugin_find_foreach, &data);
	return data.plugin;
}


static gboolean
xmms_plugin_setup (xmms_plugin_t *plugin, const xmms_plugin_desc_t *desc)
{
	plugin->type = desc->type;
	plugin->shortname = desc->shortname;
	plugin->name = desc->name;
	plugin->version = desc->version;
	plugin->description = desc->description;

	return TRUE;
}

void
xmms_plugin_destroy (xmms_plugin_t *plugin)
{
	if (plugin->module)
		g_module_close (plugin->module);
}
