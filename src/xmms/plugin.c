#include "plugin.h"
#include "plugin_int.h"
#include "util.h"

#include <gmodule.h>
#include <string.h>

struct xmms_plugin_St {
	GMutex *mtx;
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

inline static void xmms_plugin_lock (xmms_plugin_t *plugin);
inline static void xmms_plugin_unlock (xmms_plugin_t *plugin);

/*
 * Public functions
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

	plugin->mtx = g_mutex_new ();
	plugin->type = type;
	plugin->name = g_strdup (name);
	plugin->shortname = g_strdup (shortname);
	plugin->description = g_strdup (description);
	plugin->method_table = g_hash_table_new (g_str_hash, g_str_equal);
	plugin->info_list = g_list_alloc ();

	return plugin;
}

void
xmms_plugin_method_add (xmms_plugin_t *plugin, const gchar *name,
						xmms_plugin_method_t method)
{
	g_return_if_fail (plugin);
	g_return_if_fail (name);
	g_return_if_fail (method);

	xmms_plugin_lock (plugin);
	g_hash_table_insert (plugin->method_table, g_strdup (name), method);
	xmms_plugin_unlock (plugin);
}

void
xmms_plugin_properties_add (xmms_plugin_t* const plugin, gint property)
{
	g_return_if_fail (plugin);
	g_return_if_fail (property);

	plugin->properties |= property;

}

void
xmms_plugin_properties_remove (xmms_plugin_t* const plugin, gint property)
{
	g_return_if_fail (plugin);
	g_return_if_fail (property);

	plugin->properties &= ~property;

}

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


/*
 * Private functions
 */

gboolean
xmms_plugin_init (void)
{
	xmms_plugin_mtx = g_mutex_new ();

	xmms_plugin_scan_directory (PKGLIBDIR);
	
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

	XMMS_DBG ("Scanning directory: %s", dir);
	
	d = g_dir_open (dir, 0, NULL);
	if (!d) {
		XMMS_DBG ("Failed to open directory");
		return;
	}

	g_mutex_lock (xmms_plugin_mtx);
	while ((name = g_dir_read_name (d))) {
		if (strncmp (name, "lib", 3) != 0)
			continue;

		if (!strstr (name, ".so"))
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

	xmms_plugin_lock (plugin);
	plugin->users++;
	xmms_plugin_unlock (plugin);
}

void
xmms_plugin_unref (xmms_plugin_t *plugin)
{
	g_return_if_fail (plugin);

	xmms_plugin_lock (plugin);
	if (plugin->users > 0) {
		plugin->users--;
	} else {
		g_warning ("Tried to unref plugin %s with 0 users", plugin->name);
	}
	xmms_plugin_unlock (plugin);
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

	xmms_plugin_lock (plugin);
	ret = g_hash_table_lookup (plugin->method_table, method);
	xmms_plugin_unlock (plugin);

	return ret;
}

/*
 * Static functions
 */

inline static void 
xmms_plugin_lock (xmms_plugin_t *plugin)
{
	g_return_if_fail (plugin);

	g_mutex_lock (plugin->mtx);
}

inline static void 
xmms_plugin_unlock (xmms_plugin_t *plugin)
{
	g_return_if_fail (plugin);

	g_mutex_unlock (plugin->mtx);
}

