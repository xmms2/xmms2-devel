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
 * Global config
 */

xmms_config_t *global_config;

/*
 * Config functions
 */

static void
add_value (gchar *key, gchar *value)
{
	xmms_config_value_t *val;
	gchar *mykey = g_strdup (key);

	val = xmms_config_value_new (mykey);
	xmms_config_value_data_set (val, value);

	g_hash_table_insert (global_config->values, mykey, val);
}

static void
parse_plugin (gchar *pluginname, xmlNodePtr n)
{
	xmlNodePtr node;

	for (node = n; node; node = node->next) {

		if (g_strcasecmp ("value", (gchar *) node->name) == 0) {
			gchar *valname;
			gchar *name = (gchar *)xmlGetProp (node, (xmlChar *)"name");

			if (name && node->children) {
				valname = g_strdup_printf ("%s.%s", pluginname, name);
				add_value (valname, g_strdup ((gchar *) node->children->content));
			}
		}

	}

}

static void
parse_section (char *sectionname, xmlNodePtr n)
{
	xmlNodePtr node;

	g_mutex_lock (global_config->mutex);

	for (node = n; node; node = node->next) {
		if (g_strcasecmp ("plugin", (char*) node->name) == 0) {
			gchar *pluginname;
			gchar *name = (gchar *)xmlGetProp (node, (xmlChar *)"name");

			if (name && node->children) {
				pluginname = g_strdup_printf ("%s.%s", sectionname, name);
				global_config->plugins = g_list_append (global_config->plugins, name);
				parse_plugin (pluginname, node->children);
			}

		} else if (g_strcasecmp ("value", (char*) node->name) == 0) {
			gchar *valname; 
			gchar *name = (gchar *)xmlGetProp (node, (xmlChar *)"name");
			if (name && node->children) {
				valname = g_strdup_printf ("%s.%s", sectionname, name);
				add_value (valname, g_strdup ((gchar *) node->children->content));
			}

		}
	}

	g_mutex_unlock (global_config->mutex);
}

gboolean
xmms_config_init (const gchar *filename)
{
	xmlDocPtr doc;
	xmlNodePtr node,root;
	xmms_config_t *config;

	g_return_val_if_fail (filename, FALSE);

	config = g_new0 (xmms_config_t, 1);
	config->mutex = g_mutex_new ();
	config->values = g_hash_table_new (g_str_hash, g_str_equal);
	config->url = g_strdup (filename);

	if(!(doc = xmlParseFile(filename))) {
		return FALSE;
	}
	
	global_config = config;
	
	root = xmlDocGetRootElement (doc);
	root = root->children;

	for (node = root; node; node = node->next) {
		if (g_strcasecmp ("section", (char *)node->name) == 0) {
			parse_section ((char*)xmlGetProp (node, "name"), node->children);
		}
	}

	return TRUE;
}

void
xmms_config_free (void)
{
	g_return_if_fail (global_config);
}

xmms_config_value_t *
xmms_config_lookup (const gchar *path)
{
	xmms_config_value_t *value;
	g_return_val_if_fail (global_config, NULL);
	
	g_mutex_lock (global_config->mutex);
	value = g_hash_table_lookup (global_config->values, path);
	g_mutex_unlock (global_config->mutex);

	return value;
}

static void
add_to_list_foreach (gpointer key, gpointer value, gpointer udata)
{
	*(GList**)udata = g_list_append (*(GList**)udata, (gchar*) key);
}

static int
common_chars (gchar *n1, gchar *n2)
{
	gint i, j = 0;
	gchar **tmp = g_strsplit (n1, ".", 0);

	while (tmp[j])
		j++;
	
	for (i = 0; i < strlen (n1); i++) {
		if (n1[i] == '.') {
			if (j == 0)
				return i;
			else
				j--;
		}

		if (n1[i] != n2[i])
			return i;
	}

	return 0;
}

gboolean
xmms_config_save (const gchar *file)
{
	GList *list = NULL;
	GList *n;
	gchar *last = NULL;
	guint lastn = 0;
	FILE *fp = NULL;

	g_return_val_if_fail (global_config, FALSE);

	g_hash_table_foreach (global_config->values, add_to_list_foreach, &list);

	list = g_list_sort (list, (GCompareFunc) g_strcasecmp);

	if (!(fp = fopen (file, "wb+"))) {
		XMMS_DBG ("Couldnt open %s for writing.", file);
		return FALSE;
	}

	fprintf (fp, "<xmms>\n");

	for (n = list; n; n = g_list_next (n)) {
		gint nume = 0;
		gchar *line = (gchar *)n->data;
		gchar **tmp = g_strsplit ((gchar *)n->data, ".", 0);

//		printf ("%s\n", line);

		while (tmp[nume])
			nume ++;
		
		if (!last) {
			fprintf (fp, "\t<section name=\"%s\">\n", tmp[0]);
			if (nume == 3) {
				fprintf (fp, "\t\t<plugin name=\"%s\">\n", tmp[1]);
				fprintf (fp, "\t\t\t<value name=\"%s\">", tmp[2]);
			} else {
				fprintf (fp, "\t\t\t<value name=\"%s\">", tmp[1]);
			}
		} else {
			gchar **tmpv;
			gchar *data;
			gint nume2 = 0;
			gint c = common_chars (last, line);
			tmpv = g_strsplit (line+c, ".", 0);
			
			while (tmpv[nume2])
				nume2 ++;

			fprintf (fp, "</value>\n");
			if (nume == 3 && lastn == 3 && nume2 >= 2) {
				/* closing a plugin */
				fprintf (fp, "\t\t</plugin>\n");
			} 

			if (nume == nume2)
				fprintf (fp, "\t</section>\n");

			if (nume == nume2)
				fprintf (fp, "\t<section name=\"%s\">\n", tmp[0]);

			if (nume == 3 && nume2 > 1) {
				fprintf (fp, "\t\t<plugin name=\"%s\">\n", tmp[1]);
			}

			data = xmms_config_value_string_get (xmms_config_lookup (line));
			
			fprintf (fp, "\t\t\t<value name=\"%s\">%s", tmp[2] ? tmp[2] : tmp[1], data);


//			printf ("%d %d\n", nume2, nume);

		}

		last = (gchar *)n->data;
		lastn = nume;

		if (!g_list_next (n)) {
			if (nume == 3) {
				fprintf (fp, "</value>\n\t\t</plugin>\n\t</section>\n");
			} else if (nume == 2)
				fprintf (fp, "</value>\n\t</section>\n");
		}
	}

	fprintf (fp, "</xmms>\n");

	fclose (fp);

	return TRUE;
}

GList *
xmms_config_plugins_get (void)
{
	GList *plugins;
	g_return_val_if_fail (global_config, NULL);

	g_mutex_lock (global_config->mutex);
	plugins = global_config->plugins;
	g_mutex_unlock (global_config->mutex);

	return plugins;
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

	XMMS_DBG ("setting %s to %s", val->name, data);

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

void
xmms_config_value_callback_set (xmms_config_value_t *val,
				xmms_object_handler_t cb,
				gpointer userdata)
{
	g_return_if_fail (val);

	if (!cb)
		return;

	xmms_object_connect (XMMS_OBJECT (val), 
			     XMMS_SIGNAL_CONFIG_VALUE_CHANGE, 
			     (xmms_object_handler_t) cb, userdata);
}

xmms_config_value_t *
xmms_config_value_register (const gchar *path, 
			    const gchar *default_value,
			    xmms_object_handler_t cb,
			    gpointer userdata)
{

	xmms_config_value_t *val;

	XMMS_DBG ("Registring: %s", path);

	g_mutex_lock (global_config->mutex);

	val = g_hash_table_lookup (global_config->values, path);
	if (!val) {
		gchar *name = strrchr (path, '.');

		if (!name) 
			val = xmms_config_value_new (path);
		else
			val = xmms_config_value_new (name+1);

		xmms_config_value_data_set (val, g_strdup (default_value));
	}

	if (cb) 
		xmms_config_value_callback_set (val, cb, userdata);

	g_hash_table_insert (global_config->values, g_strdup (path), val);

	g_mutex_unlock (global_config->mutex);

	return val;
}
