#include <glib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdlib.h>
#include <string.h>

#include "xmms/config.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/signal_xmms.h"
#include "xmms/plugin.h"


struct xmms_config_St {
	const gchar *url;

	GHashTable *values;
	GList *plugins;

	GMutex *mutex;
};

struct xmms_config_value_St {
	xmms_object_t obj;

	const gchar *name;
	gchar *data;
	
};

/*
 * Config functions
 */

static void
add_value (xmms_config_t *config, gchar *key, gchar *value)
{
	xmms_config_value_t *val;
	gchar *mykey = g_strdup (key);

	val = xmms_config_value_new (mykey);
	xmms_config_value_data_set (val, value);
	g_hash_table_insert (config->values, mykey, val);
}

static void
parse_plugin (gchar *pluginname, xmms_config_t *config, xmlNodePtr n)
{
	xmlNodePtr node;

	for (node = n; node; node = node->next) {

		if (g_strcasecmp ("value", (gchar *) node->name) == 0) {
			gchar *valname;
			gchar *name = (gchar *)xmlGetProp (node, (xmlChar *)"name");

			if (name && node->children) {
				valname = g_strdup_printf ("%s.%s", pluginname, name);
				add_value (config, valname, g_strdup ((gchar *) node->children->content));
			}
		}

	}

}

static void
parse_section (char *sectionname, xmms_config_t *config, xmlNodePtr n)
{
	xmlNodePtr node;

	for (node = n; node; node = node->next) {
		if (g_strcasecmp ("plugin", (char*) node->name) == 0) {
			gchar *pluginname;
			gchar *name = (gchar *)xmlGetProp (node, (xmlChar *)"name");

			if (name && node->children) {
				pluginname = g_strdup_printf ("%s.%s", sectionname, name);
				config->plugins = g_list_append (config->plugins, name);
				parse_plugin (pluginname, config, node->children);
			}

		} else if (g_strcasecmp ("value", (char*) node->name) == 0) {
			gchar *valname; 
			gchar *name = (gchar *)xmlGetProp (node, (xmlChar *)"name");
			if (name && node->children) {
				valname = g_strdup_printf ("%s.%s", sectionname, name);
				add_value (config, valname, g_strdup ((gchar *) node->children->content));
			}

		}
	}
}

xmms_config_t *
xmms_config_init (const gchar *filename)
{
	xmlDocPtr doc;
	xmlNodePtr node,root;
	xmms_config_t *config;

	g_return_val_if_fail (filename, NULL);

	config = g_new0 (xmms_config_t, 1);
	config->mutex = g_mutex_new ();
	config->values = g_hash_table_new (g_str_hash, g_str_equal);
	config->url = g_strdup (filename);

	if(!(doc = xmlParseFile(filename))) {
		return NULL;
	}
	
	root = xmlDocGetRootElement (doc);
	root = root->children;

	for (node = root; node; node = node->next) {
		if (g_strcasecmp ("section", (char *)node->name) == 0) {
			parse_section ((char*)xmlGetProp (node, "name"), config, node->children);
		}
	}
	
	return config;
}

void
xmms_config_free (xmms_config_t *config)
{
	g_return_if_fail (config);
}

xmms_config_value_t *
xmms_config_lookup (xmms_config_t *config, 
		    const gchar *path)
{
	g_return_val_if_fail (config, NULL);

	return g_hash_table_lookup (config->values, path);
}

void
print_foreach (gpointer key, gpointer value, gpointer udata)
{
	printf ("%s = %s\n", (gchar*)key, xmms_config_value_string_get ((xmms_config_value_t *)value));
}

gboolean
xmms_config_save (xmms_config_t *config)
{
	g_return_val_if_fail (config, FALSE);

	g_hash_table_foreach (config->values, print_foreach, NULL);

	return TRUE;
}

GList *
xmms_config_plugins_get (xmms_config_t *config)
{
	g_return_val_if_fail (config, NULL);

	return config->plugins;
}

/*
 * Value manipulation 
 */

xmms_config_value_t *
xmms_config_value_new (const gchar *name)
{
	xmms_config_value_t *ret;

	ret = g_new0 (xmms_config_value_t, 1);
	ret->name = name;

	XMMS_DBG ("adding config value %s", name);
	xmms_object_init (XMMS_OBJECT (ret));

	return ret;
}

const gchar *
xmms_config_value_name_get (const xmms_config_value_t *value)
{
	g_return_val_if_fail (value, NULL);

	return value->name;
}

void
xmms_config_value_free (xmms_config_value_t *value)
{
	g_return_if_fail (value);
	g_free (value);
}

void
xmms_config_value_data_set (xmms_config_value_t *val, gchar *data)
{
	g_return_if_fail (val);
	g_return_if_fail (data);

	val->data = data;
	xmms_object_emit (XMMS_OBJECT (val), XMMS_SIGNAL_CONFIG_VALUE_CHANGE,
			  (gpointer) data);
}

const gchar *
xmms_config_value_string_get (const xmms_config_value_t *val)
{
	g_return_val_if_fail (val, NULL);
	return val->data;
}

gint
xmms_config_value_int_get (const xmms_config_value_t *val)
{
	g_return_val_if_fail (val, 0);
	if (val->data)
		return atoi (val->data);

	return 0;
}

gfloat
xmms_config_value_float_get (const xmms_config_value_t *val)
{
	g_return_val_if_fail (val, 0.0);
	if (val->data)
		return atof (val->data);

	return 0.0;
}

/*
 * Plugin configcode.
 */

xmms_config_value_t *
xmms_config_value_register (xmms_config_t *config,
			    const gchar *path, 
			    const gchar *default_value,
			    xmms_object_handler_t cb,
			    gpointer userdata)
{

	xmms_config_value_t *val;

	val = g_hash_table_lookup (config->values, path);
	if (!val) {
		gchar *name = strrchr (path, '.');

		if (!name) 
			val = xmms_config_value_new (path);
		else
			val = xmms_config_value_new (name+1);

		xmms_config_value_data_set (val, g_strdup (default_value));
	}

	if (cb) 
		xmms_object_connect (XMMS_OBJECT (val), XMMS_SIGNAL_CONFIG_VALUE_CHANGE, (xmms_object_handler_t)cb, userdata);

	g_hash_table_insert (config->values, g_strdup (path), val);

	return val;
}




