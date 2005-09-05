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

#include "xmms/xmms_defs.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_log.h"
#include "xmmspriv/xmms_magic.h"

#include <gmodule.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_VALGRIND
# include <memcheck.h>
#endif

#ifdef XMMS_OS_DARWIN
# define XMMS_LIBSUFFIX ".dylib"
#else
# define XMMS_LIBSUFFIX ".so"
#endif

typedef struct {
	gchar *key;
	gchar *value;
} xmms_plugin_info_t;

extern xmms_config_t *global_config;

struct xmms_plugin_St {
	xmms_object_t object;
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

	/* decoder/playlist plugin specific stuff */
	GList *magic;
};

/*
 * Global variables
 */

static GMutex *xmms_plugin_mtx;
static GList *xmms_plugin_list;

/*
 * Function prototypes
 */

static void xmms_plugin_destroy (xmms_object_t *object);
static gchar *plugin_config_path (xmms_plugin_t *plugin, const gchar *value);

/*
 * Public functions
 */

/**
 * @defgroup XMMSPlugin XMMSPlugin
 * @brief Functions used when working with XMMS2 plugins in general.
 *
 * Every plugin has an init function that should be defined as follows:
 * @code
 * xmms_plugin_t *xmms_plugin_get (void)
 * @endcode
 * 
 * This function must call #xmms_plugin_new with the appropiate arguments. 
 * This function can also call #xmms_plugin_info_add, #xmms_plugin_method_add,
 * #xmms_plugin_properties_add
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
 * @if api_plugin
 * @{
 */

/**
 * Create a new plugin instance.
 *
 * @param[in] type The type of plugin. All plugin types are enumerated in
 * #xmms_plugin_type_t.
 * @param[in] shortname The short version of plugin name. Eg. "oss"
 * @param name[in] The full name of the plugin. Eg. "Open Sound System"
 * @param[in] description Text describing plugin.
 *
 * @return a new plugin of the given type.
 */
xmms_plugin_t *
xmms_plugin_new (xmms_plugin_type_t type, 
		 gint api_version,
		 const gchar *shortname,
		 const gchar *name,
		 const gchar *description)
{
	xmms_plugin_t *plugin;
	gboolean api_mismatch = FALSE;

	g_return_val_if_fail (name, NULL);
	g_return_val_if_fail (description, NULL);

	switch (type) {
		case XMMS_PLUGIN_TYPE_TRANSPORT:
			if (api_version != XMMS_TRANSPORT_PLUGIN_API_VERSION)
				api_mismatch = TRUE;
			break;
		case XMMS_PLUGIN_TYPE_DECODER:
			if (api_version != XMMS_DECODER_PLUGIN_API_VERSION)
				api_mismatch = TRUE;
			break;
		case XMMS_PLUGIN_TYPE_OUTPUT:
			if (api_version != XMMS_OUTPUT_PLUGIN_API_VERSION)
				api_mismatch = TRUE;
			break;
		case XMMS_PLUGIN_TYPE_PLAYLIST:
			if (api_version != XMMS_PLAYLIST_PLUGIN_API_VERSION)
				api_mismatch = TRUE;
			break;
		case XMMS_PLUGIN_TYPE_EFFECT:
			if (api_version != XMMS_EFFECT_PLUGIN_API_VERSION)
				api_mismatch = TRUE;
			break;
		case XMMS_PLUGIN_TYPE_ALL:
			break;
	}

	if (api_mismatch) {
		xmms_log_error ("API VERSION MISMATCH FOR PLUGIN %s!", name);
		return NULL;
	}

	plugin = xmms_object_new (xmms_plugin_t, xmms_plugin_destroy);

	plugin->mutex = g_mutex_new ();
	plugin->type = type;
	plugin->name = g_strdup (name);
	plugin->shortname = g_strdup (shortname);
	plugin->description = g_strdup (description);
	plugin->method_table = g_hash_table_new_full (g_str_hash,
	                                              g_str_equal,
	                                              g_free, NULL);

	return plugin;
}

/**
 * Add a method to this plugin. Different types of plugins may add different
 * types of methods.
 *
 * @param[in] plugin A new plugin created from #xmms_plugin_new
 * @param[in] name The name of the method to add. Allowable method names
 * are defined in xmms_plugin.h and have the prefix 'XMMS_PLUGIN_METHOD_'.
 * @param[in] method The function pointer to the method.
 */
void
__xmms_plugin_method_add (xmms_plugin_t *plugin, const gchar *name,
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
 * The different properties are defined under each plugin type's documentation.
 * @param[in] plugin The plugin
 * @param[in] property The property to add
 */
void
xmms_plugin_properties_add (xmms_plugin_t* const plugin, gint property)
{
	g_return_if_fail (plugin);
	g_return_if_fail (property);

	plugin->properties |= property;

}

/**
 * Remove a property from this plugin.
 * @param[in] plugin The plugin
 * @param[in] The property to remove
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
	info->key = g_strdup (key);
	info->value = g_strdup (value);

	plugin->info_list = g_list_append (plugin->info_list, info);
}

gboolean
xmms_plugin_magic_add (xmms_plugin_t *plugin, const gchar *desc,
                       const gchar *mime, ...)
{
	GNode *tree, *node = NULL;
	va_list ap;
	gchar *s;
	gpointer *root_props;
	gboolean ret = TRUE;

	g_return_val_if_fail (plugin, FALSE);
	g_return_val_if_fail (plugin->type == XMMS_PLUGIN_TYPE_DECODER ||
	                      plugin->type == XMMS_PLUGIN_TYPE_PLAYLIST,
	                      FALSE);
	g_return_val_if_fail (desc, FALSE);
	g_return_val_if_fail (mime, FALSE);

	/* now process the magic specs in the argument list */
	va_start (ap, mime);

	s = va_arg (ap, gchar *);
	if (!s) { /* no magic specs passed -> failure */
		va_end (ap);
		return FALSE;
	}

	/* root node stores the description and the mimetype */
	root_props = g_new0 (gpointer, 2);
	root_props[0] = g_strdup (desc);
	root_props[1] = g_strdup (mime);
	tree = g_node_new (root_props);

	do {
		if (!strlen (s)) {
			ret = FALSE;
			xmms_log_error ("invalid magic spec: '%s'", s);
			break;
		}

		s = g_strdup (s); /* we need our own copy */
		node = xmms_magic_add (tree, s, node);
		g_free (s);

		if (!node) {
			xmms_log_error ("invalid magic spec: '%s'", s);
			ret = FALSE;
			break;
		}
	} while ((s = va_arg (ap, gchar *)));

	va_end (ap);

	/* only add this tree to the list if all spec chunks are valid */
	if (ret) {
		plugin->magic = g_list_append (plugin->magic, tree);
	} else {
		xmms_magic_tree_free (tree);
	}

	return ret;
}

/**
 * Lookup the value of a plugin's config property, given the property key.
 * @param[in] plugin The plugin
 * @param[in] value The property key (config path)
 * @return A config value
 * @todo config value <-> property fixup
 */
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

/**
 * Register a config property for a plugin.
 * @param[in] plugin The plugin
 * @param[in] value The property name
 * @param[in] default_value The default value for the property
 * @param[in] cb A callback function to be executed when the property value
 * changes
 * @param[in] userdata Pointer to data to be passed to the callback
 * @todo config value <-> property fixup
 */
xmms_config_value_t *
xmms_plugin_config_value_register (xmms_plugin_t *plugin,
				   const gchar *value,
				   const gchar *default_value,
				   xmms_object_handler_t cb,
				   gpointer userdata)
{
	gchar *fullpath;
	xmms_config_value_t *val;

	g_return_val_if_fail (plugin, NULL);
	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (default_value, NULL);

	fullpath = plugin_config_path (plugin, value);

	val = xmms_config_value_register (fullpath, 
					  default_value, 
					  cb, userdata);

	/* xmms_config_value_register() copies the given path,
	 * so we have to free our copy here
	 */
	g_free (fullpath);

	return val;
}

/**
 * @}
 * -- end of plugin API section --
 * @endif
 * 
 * @if internal
 * -- internal documentation section --
 * @addtogroup XMMSPlugin
 * @{
 */

/**
 * @internal Check whether plugin has a property
 * @param[in] plugin The plugin
 * @param[in] property The property to check for
 * @return TRUE if plugin has property
 */
gboolean
xmms_plugin_properties_check (const xmms_plugin_t *plugin, gint property)
{
	g_return_val_if_fail (plugin, FALSE);
	g_return_val_if_fail (property, FALSE);

	return plugin->properties & property;

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

/**
 * @internal Get info from the plugin.
 * @param[in] plugin The plugin
 * @return a GList of info from the plugin
 */
const GList*
xmms_plugin_info_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->info_list;
}

/**
 * @internal Get magic specs from the plugin.
 * @param[in] plugin The plugin
 * @return a GList of magic specs from the plugin
 */
const GList *
xmms_plugin_magic_get (const xmms_plugin_t *plugin)
{
	g_return_val_if_fail (plugin, NULL);

	return plugin->magic;
}

/*
 * Private functions
 */

/**
 * @internal Initialise the plugin system
 * @param[in] path Absolute path to the plugins directory.
 * @return Whether the initialisation was successful or not.
 */
gboolean
xmms_plugin_init (gchar *path)
{
	xmms_plugin_mtx = g_mutex_new ();

	if (!path)
		path = PKGLIBDIR;

	return xmms_plugin_scan_directory (path);
	
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
#endif

	/* at this point, there's only one thread left,
	 * so we don't need to take care of the mutex here.
	 * xmms_plugin_destroy() will try to lock it, though, so
	 * don't free the mutex yet.
	 */
	while (xmms_plugin_list) {
		xmms_plugin_t *p = xmms_plugin_list->data;

		/* if this plugin's refcount is > 1, then there's a bug
		 * in one of the other subsystems
		 */
		if (p->object.ref > 1) {
			XMMS_DBG ("%s's refcount is %i",
			          p->name, p->object.ref);
		}

		/* xmms_plugin_destroy() will remove the plugin from
		 * xmms_plugin_list, so we must not do that here
		 */
		xmms_object_unref (p);
	}

	g_mutex_free (xmms_plugin_mtx);
}

/**
 * @internal Scan a particular directory for plugins to load
 * @param[in] dir Absolute path to plugins directory
 * @return TRUE if directory successfully scanned for plugins
 */
gboolean
xmms_plugin_scan_directory (const gchar *dir)
{
	GDir *d;
	const char *name;
	gchar *path;
	GModule *module;
	xmms_plugin_t *(*plugin_init) (void);
	xmms_plugin_t *plugin;
	gpointer sym;

	g_return_val_if_fail (global_config, FALSE);

	XMMS_DBG ("Scanning directory: %s", dir);
	
	d = g_dir_open (dir, 0, NULL);
	if (!d) {
		xmms_log_error ("Failed to open directory");
		return FALSE;
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
			xmms_log_error ("Failed to open plugin %s: %s",
			                path, g_module_error ());
			g_free (path);
			continue;
		}

		if (!g_module_symbol (module, "xmms_plugin_get", &sym)) {
			xmms_log_error ("Failed to open plugin %s: "
			                "initialization function missing", path);
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

	return TRUE;
}

GList *
xmms_plugin_client_list (xmms_object_t *main, guint32 type, xmms_error_t *err)
{
	GList *list = NULL, *node, *l;

	l = xmms_plugin_list_get (type);

	g_mutex_lock (xmms_plugin_mtx);

	for (node = l; node; node = g_list_next (node)) {
		GHashTable *hash;
		const GList *p;
		xmms_plugin_t *plugin = node->data;

		hash = g_hash_table_new (g_str_hash, g_str_equal);
		g_hash_table_insert (hash, "name", 
							 xmms_object_cmd_value_str_new ((gchar *)xmms_plugin_name_get (plugin)));
		g_hash_table_insert (hash, "shortname", 
							 xmms_object_cmd_value_str_new ((gchar *)xmms_plugin_shortname_get (plugin)));
		g_hash_table_insert (hash, "description", 
							 xmms_object_cmd_value_str_new ((gchar *)xmms_plugin_description_get (plugin)));
		g_hash_table_insert (hash, "type", 
							 xmms_object_cmd_value_uint_new (xmms_plugin_type_get (plugin)));

		for (p = xmms_plugin_info_get (plugin); p; p = g_list_next (p)) {
			xmms_plugin_info_t *info = p->data;
			g_hash_table_insert (hash, info->key, xmms_object_cmd_value_str_new (info->value));
		}

		list = g_list_prepend (list, xmms_object_cmd_value_dict_new (hash));

	}

	g_mutex_unlock (xmms_plugin_mtx);

	xmms_plugin_list_destroy (l);

	return list;
}

/**
 * @internal Look for loaded plugins matching a particular type
 * @param[in] type The plugin type to look for. (#xmms_plugin_type_t)
 * @return List of loaded plugins matching type
 */
GList *
xmms_plugin_list_get (xmms_plugin_type_t type)
{
	GList *list = NULL, *node;

	g_mutex_lock (xmms_plugin_mtx);

	for (node = xmms_plugin_list; node; node = g_list_next (node)) {
		xmms_plugin_t *plugin = node->data;

		if (plugin->type == type || type == XMMS_PLUGIN_TYPE_ALL) {
			xmms_object_ref (plugin);
			list = g_list_prepend (list, plugin);
		}
	}
	
	g_mutex_unlock (xmms_plugin_mtx);
	
	return list;
}

/**
 * @internal Destroy a list of plugins. Note: this is not used to destroy the
 * global plugin list.
 * @param[in] list The plugin list to destroy
 */
void
xmms_plugin_list_destroy (GList *list)
{
	while (list) {
		xmms_object_unref (list->data);
		list = g_list_delete_link (list, list);
	}
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
	xmms_plugin_t *ret = NULL;
	GList *l;

	g_return_val_if_fail (name, NULL);

	g_mutex_lock (xmms_plugin_mtx);

	for (l = xmms_plugin_list; l; l = l->next) {
		xmms_plugin_t *plugin = l->data;

		if (plugin->type == type &&
		    !g_strcasecmp (plugin->shortname, name)) {
			ret = plugin;
			xmms_object_ref (ret);

			break;
		}
	}

	g_mutex_unlock (xmms_plugin_mtx);

	return ret;
}

/**
 * @internal Get a pointer to a particularly named method in the plugin
 * @param[in] plugin The plugin
 * @param[in] method The method name to look for
 * @return Pointer to plugin method
 */
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

/*
 * Static functions
 */

/**
 * @internal Destroy a particular instance of a plugin.
 * @param[in] object The plugin (object) to destroy.
 */
static void
xmms_plugin_destroy (xmms_object_t *object)
{
	xmms_plugin_t *p = (xmms_plugin_t *) object;

	g_mutex_free (p->mutex);
	g_module_close (p->module);
	g_free (p->name);
	g_free (p->shortname);
	g_free (p->description);
	g_hash_table_destroy (p->method_table);

	while (p->info_list) {
		xmms_plugin_info_t *info = p->info_list->data;

		g_free (info->key);
		g_free (info->value);
		g_free (info);

		p->info_list = g_list_delete_link (p->info_list, p->info_list);
	}

	while (p->magic) {
		xmms_magic_tree_free (p->magic->data);
		p->magic = g_list_delete_link (p->magic, p->magic);
	}

	/* remove this plugin from the global plugin list */
	g_mutex_lock (xmms_plugin_mtx);
	xmms_plugin_list = g_list_remove (xmms_plugin_list, p);
	g_mutex_unlock (xmms_plugin_mtx);
}

/**
 * @internal Build 'absolute' config path to a plugin's config property,
 * given the plugin and the property name.
 * @param[in] plugin The plugin
 * @param[in] value The property name
 * @return Config path (property key) as string
 * @todo config value <-> property fixup
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

/**
 * @}
 * @endif
 */
