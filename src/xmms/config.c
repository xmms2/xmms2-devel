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

#include <glib.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "xmms/config.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/signal_xmms.h"
#include "xmms/plugin.h"
#include "xmms/dbus.h"

/**
  * @defgroup Config Config
  * @brief controls configuration for the server.
  * 
  * The configuration is saved and loaded from a XML file. Its splitted in a plugin part and a
  * core part. This documents the configuration for parts inside the server. For plugin config
  * see each server objects documentation.
  *
  * @ingroup XMMSServer
  * @{
  */

struct xmms_config_St {
	xmms_object_t obj;
	const gchar *url;

	GHashTable *values;
	GList *plugins;

	GMutex *mutex;

	/* parsing */
	gint state;
	gint prevstate;
	gchar *where[3];
};

struct xmms_config_value_St {
	xmms_object_t obj;

	const gchar *name;
	gchar *data;
	
};


/** @internal */
enum xmms_config_states {
	XMMS_CONFIG_STATE_START,
	XMMS_CONFIG_STATE_SECTION,
	XMMS_CONFIG_STATE_PLUGIN,
	XMMS_CONFIG_STATE_VALUE,
};

/**
  * Global config
  * Since there can only be one configuration per server
  * we can have the convineance to have it global.
  */

xmms_config_t *global_config;


void xmms_config_setvalue (xmms_config_t *conf, gchar *key, gchar *value, xmms_error_t *err);
GList *xmms_config_listvalues (xmms_config_t *conf, xmms_error_t *err);
static xmms_config_value_t *xmms_config_value_new (const gchar *name);

/*
 * Config functions
 */

static void
add_to_list_foreach (gpointer key, gpointer value, gpointer udata)
{
	*(GList**)udata = g_list_append (*(GList**)udata, (gchar*) key);
}

static void 
xmms_config_parse_start (GMarkupParseContext *ctx,
		    	 const gchar *name,
		    	 const gchar **attr_name,
		    	 const gchar **attr_data,
		    	 gpointer userdata,
		    	 GError **error)
{
	xmms_config_t *st = userdata;

	if (g_strcasecmp (name, "section") == 0) {
		st->state = XMMS_CONFIG_STATE_SECTION;
		st->where[0] = g_strdup (attr_data[0]);
	} else if (g_strcasecmp (name, "plugin") == 0) {
		if (st->state == XMMS_CONFIG_STATE_SECTION) {
			st->prevstate = st->state;
			st->state = XMMS_CONFIG_STATE_PLUGIN;
			st->where[1] = g_strdup (attr_data[0]);
		} else {
			xmms_log_fatal ("Config file is a mess");
		}
	} else if (g_strcasecmp (name, "value") == 0) {
		if (st->state == XMMS_CONFIG_STATE_SECTION) {
			st->prevstate = st->state;
			st->state = XMMS_CONFIG_STATE_VALUE;
			st->where[1] = g_strdup (attr_data[0]);
		} else if (st->state == XMMS_CONFIG_STATE_PLUGIN) {
			st->prevstate = st->state;
			st->state = XMMS_CONFIG_STATE_VALUE;
			st->where[2] = g_strdup (attr_data[0]);
		} else {
			xmms_log_fatal ("Config file is a mess");
		}
	} 

}

void 
xmms_config_parse_end (GMarkupParseContext *ctx,
		  	const gchar *name,
		  	gpointer userdata,
		  	GError **err)
{
	xmms_config_t *st = userdata;

	switch (st->state) {
		case XMMS_CONFIG_STATE_SECTION:
			g_free (st->where[0]);
			st->where[0] = NULL;
			st->state = XMMS_CONFIG_STATE_START;
			break;
		case XMMS_CONFIG_STATE_PLUGIN:
			g_free (st->where[1]);
			st->where[1] = NULL;
			st->state = XMMS_CONFIG_STATE_SECTION;
			break;
		case XMMS_CONFIG_STATE_VALUE:
			if (st->prevstate == XMMS_CONFIG_STATE_SECTION) {
				g_free (st->where[1]);
				st->where[1] = NULL;
				st->state = XMMS_CONFIG_STATE_SECTION;
			} else {
				g_free (st->where[2]);
				st->where[2] = NULL;
				st->state = XMMS_CONFIG_STATE_PLUGIN;
			}
			break;
	}

}

static void
add_value (gchar *key, gchar *value)
{
	xmms_config_value_t *val;

	val = xmms_config_value_new (key);
	xmms_config_value_data_set (val, value);

	g_hash_table_insert (global_config->values, key, val);
}

static void
xmms_config_parse_text (GMarkupParseContext *ctx,
      			const gchar *text,
      			gsize text_len,
      			gpointer userdata,
      			GError **err)
{
	xmms_config_t *st = userdata;
	gchar *str;

	if (st->state != XMMS_CONFIG_STATE_VALUE)
		return;

	if (st->prevstate == XMMS_CONFIG_STATE_SECTION) {
		str = g_strdup_printf ("%s.%s", st->where[0], st->where[1]);
	} else 
		str = g_strdup_printf ("%s.%s.%s", st->where[0], 
						   st->where[1], 
						   st->where[2]);

	add_value (str, g_strdup (text));
}



/**
 * Sets a key to a new value
 */

XMMS_METHOD_DEFINE (setvalue, xmms_config_setvalue, xmms_config_t *, NONE, STRING, STRING);
void
xmms_config_setvalue (xmms_config_t *conf, gchar *key, gchar *value, xmms_error_t *err)
{
	xmms_config_value_t *val;

	val = xmms_config_lookup (key);
	if (val) {
		gchar cf[255];
		
		g_snprintf (cf, 254, "%s/.xmms2/xmms2.conf", g_get_home_dir ());
		
		xmms_config_value_data_set (val, g_strdup (value));
		xmms_config_save (cf);
	} else {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Trying to set nonexistant configvalue");
	}

}

XMMS_METHOD_DEFINE (listvalues, xmms_config_listvalues, xmms_config_t *, STRINGLIST, NONE, NONE);

/**
  * List all keys and values in the list.
  * @returns a GList with strings with format "key=value"
  */
GList *
xmms_config_listvalues (xmms_config_t *conf, xmms_error_t *err)
{
	GList *ret = NULL;

	XMMS_DBG ("Configvalue list");

	g_mutex_lock (conf->mutex);
	
	g_hash_table_foreach (conf->values, add_to_list_foreach, &ret);

	g_mutex_unlock (conf->mutex);

	ret = g_list_sort (ret, (GCompareFunc) g_strcasecmp);

	return ret;

}


/**
  * Lookup configvalue and return it as a string.
  * This is a convinient function for removing the step
  * with #xmms_config_value_string_get
  *
  * @returns a char with the value. If the value is a int it will return NULL
  */

const gchar *
xmms_config_value_lookup_string_get (xmms_config_t *conf, gchar *key, xmms_error_t *err)
{
	xmms_config_value_t *val;

	val = xmms_config_lookup (key);
	if (!val) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Trying to get nonexistant configvalue");
		return NULL;
	}

	return xmms_config_value_string_get (val);
}


XMMS_METHOD_DEFINE (getvalue, xmms_config_value_lookup_string_get, xmms_config_t *, STRING, STRING, NONE);

static void
xmms_config_destroy (xmms_object_t *object) 
{
	xmms_config_t *config = (xmms_config_t *)object;
	g_mutex_free (config->mutex);
	/** @todo free all values? */
	g_hash_table_destroy (config->values);
}

/**
  * Initialize and parse the configfile.
  * @param filename a string with the absolute path to a configfile
  * @returns TRUE if configfile is found and correctly parsed.
  */

gboolean
xmms_config_init (const gchar *filename)
{
	GMarkupParser pars;
	GMarkupParseContext *ctx;
	xmms_config_t *config;
	int ret, fd;

	config = xmms_object_new (xmms_config_t, xmms_config_destroy);
	config->mutex = g_mutex_new ();
	config->values = g_hash_table_new (g_str_hash, g_str_equal);
	config->state = XMMS_CONFIG_STATE_START;
	global_config = config;

	if (!filename)
		return TRUE;

	memset (&pars, 0, sizeof (pars));

	pars.start_element = xmms_config_parse_start;
	pars.end_element = xmms_config_parse_end;
	pars.text = xmms_config_parse_text;

	ctx = g_markup_parse_context_new (&pars, 0, config, NULL);

	fd = open (filename, O_RDONLY);

	if (fd == -1) return FALSE;

	while (42) {
		GError *error = NULL;
		gchar buffer[1024];

		ret = read (fd, buffer, 1024);
		if (ret < 1) {
			g_markup_parse_context_end_parse (ctx, &error);
			if (error) {
				xmms_log_error ("Cannot parse config file: %s",
				                error->message);
				g_error_free (error);
			}

			break;
		}

		g_markup_parse_context_parse (ctx, buffer, ret, &error);
		if (error) {
			xmms_log_error ("Cannot parse config file: %s",
			                error->message);
			g_error_free (error);
		}
	}

	close (fd);
	g_markup_parse_context_free (ctx);

	xmms_object_method_add (XMMS_OBJECT (config), "setvalue", XMMS_METHOD_FUNC (setvalue));
	xmms_object_method_add (XMMS_OBJECT (config), "get", XMMS_METHOD_FUNC (getvalue));
	xmms_object_method_add (XMMS_OBJECT (config), "list", XMMS_METHOD_FUNC (listvalues));
	xmms_dbus_register_object ("config", XMMS_OBJECT (config));

	return TRUE;
}

/**
  * Look up a configvalue from the global config
  * @param path a configuration path. could be core.myconfig or effect.foo.myconfig
  * @returns a newly allocated #xmms_config_value_t
  */

xmms_config_value_t *
xmms_config_lookup (const gchar *path)
{
	xmms_config_value_t *value;
	g_return_val_if_fail (global_config, NULL);
	
	XMMS_DBG ("Looking up %s", path);
	
	g_mutex_lock (global_config->mutex);
	value = g_hash_table_lookup (global_config->values, path);
	g_mutex_unlock (global_config->mutex);


	return value;
}

static GNode *
add_node (GNode *parent, gpointer new_data[2])
{
	GNode *node;
	gpointer *node_data;
	int i;

	/* loop over the childs of parent */
	for (node = g_node_first_child (parent); node;
	     node = g_node_next_sibling (node)) {
		node_data = node->data;

		i = g_ascii_strcasecmp (node_data[0], new_data[0]);
		if (!i) {
			/* the node we want already exists, so we don't need
			 * new_data anymore.
			 * note that this never happens for "value" nodes, so
			 * data[1] = ... below is safe.
			 */
			g_free (new_data[0]);
			g_free (new_data);

			return node;
		} else if (i > 0) {
			/* an exact match is impossible at this point, so create
			 * a new node and return
			 */
			return g_node_insert_data_before (parent, node, new_data);
		}
	}

	return g_node_append_data (parent, new_data);
}

static void
add_to_tree_foreach (gpointer key, gpointer value, gpointer udata)
{
	GNode *node = udata;
	gchar **subkey, **ptr;
	gpointer *data;

	subkey = g_strsplit (key, ".", 0);
	g_assert (subkey);

	for (ptr = subkey; *ptr; ptr++) {
		data = g_new0 (gpointer, 2);
		data[0] = g_strdup (*ptr);

		node = add_node (node, data);
	}

	/* only the last node gets a value */
	data[1] = g_strdup (xmms_config_value_string_get (value));

	g_strfreev (subkey);
}

static void
dump_value (GNode *value, gpointer arg[3])
{
	gchar key[1024];
	gpointer *data = value->data;

	/* assemble the key */
	g_snprintf (key, sizeof (key), "%s.", (gchar *) arg[1]);

	if (arg[2]) {
		g_strlcat (key, (gchar *) arg[2], sizeof (key));
		g_strlcat (key, ".", sizeof (key));
	}

	g_strlcat (key, (gchar *) data[0], sizeof (key));

	if (arg[2])
		fprintf (arg[0], "\t");

	fprintf (arg[0], "\t\t<value name=\"%s\">%s</value>\n",
	         (gchar *) data[0], (gchar *) data[1]);
}

static void
dump_plugin (GNode *plugin, gpointer arg[3])
{
	gpointer *data = plugin->data;

	/* if there are no child nodes here, its a "value" node, not
	 * a "plugin" node
	 */
	if (!(g_node_n_children (plugin))) {
		dump_value (plugin, arg);
		return;
	}

	fprintf (arg[0], "\t\t<plugin name=\"%s\">\n", (gchar *) data[0]);

	arg[2] = data[0];
	g_node_children_foreach (plugin, G_TRAVERSE_ALL,
	                         (GNodeForeachFunc) dump_value, arg);
	arg[2] = NULL;

	fprintf (arg[0], "\t\t</plugin>\n");
}

static void
dump_section (GNode *section, FILE *fp)
{
	gpointer *data, arg[3];

	data = section->data;
	fprintf (fp, "\t<section name=\"%s\">\n", (gchar *) data[0]);

	arg[0] = fp;
	arg[1] = data[0];
	arg[2] = NULL; /* filled later */
	g_node_children_foreach (section, G_TRAVERSE_ALL,
	                         (GNodeForeachFunc) dump_plugin, arg);

	fprintf (fp, "\t</section>\n");
}

static void
dump_tree (GNode *tree, FILE *fp)
{
	fprintf (fp, "<?xml version=\"1.0\"?>\n");

	fprintf (fp, "<%s>\n", (gchar *) tree->data);
	g_node_children_foreach (tree, G_TRAVERSE_ALL,
	                         (GNodeForeachFunc) dump_section, fp);
	fprintf (fp, "</%s>\n", (gchar *) tree->data);
}

static gboolean
destroy_node (GNode *node, gpointer udata)
{
	gpointer *data = node->data;

	/* the root just carries a single string */
	if (!G_NODE_IS_ROOT (node)) {
		g_free (data[0]);
		g_free (data[1]);
	}

	g_free (data);

	return FALSE; /* go on */
}

/**
  * Save the global configuration to disk.
  * @param file absolute path to configfile. This will be overwritten.
  * @returns #TRUE on success.
  */

gboolean
xmms_config_save (const gchar *file)
{
	GNode *tree;
	FILE *fp = NULL;

	g_return_val_if_fail (global_config, FALSE);
	g_return_val_if_fail (file, FALSE);

	tree = g_node_new (g_strdup ("xmms"));
	g_hash_table_foreach (global_config->values, add_to_tree_foreach, tree);

	if (!(fp = fopen (file, "w"))) {
		xmms_log_error ("Couldn't open %s for writing.", file);
		return FALSE;
	}

	dump_tree (tree, fp);
	fclose (fp);

	/* destroy the tree */
	g_node_traverse (tree, G_IN_ORDER, G_TRAVERSE_ALL, -1,
	                 destroy_node, NULL);

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

static xmms_config_value_t *
xmms_config_value_new (const gchar *name)
{
	xmms_config_value_t *ret;

	ret = xmms_object_new (xmms_config_value_t, NULL);
	ret->name = name;

	return ret;
}

/**
  * Returns the name of this value.
  */

const gchar *
xmms_config_value_name_get (const xmms_config_value_t *value)
{
	g_return_val_if_fail (value, NULL);

	return value->name;
}

/**
  * Set the data of the valuestruct to a new value
  */

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

/**
  * Return the value in the struct as a string
  */

const gchar *
xmms_config_value_string_get (const xmms_config_value_t *val)
{
	g_return_val_if_fail (val, NULL);
	return val->data;
}

/**
  * Return the value in the struct as a int
  */

gint
xmms_config_value_int_get (const xmms_config_value_t *val)
{
	g_return_val_if_fail (val, 0);
	if (val->data)
		return atoi (val->data);

	return 0;
}

/**
  * Return the value in the struct as a float
  */

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

void
xmms_config_value_callback_remove (xmms_config_value_t *val,
                                   xmms_object_handler_t cb)
{
	g_return_if_fail (val);

	if (!cb)
		return;

	xmms_object_disconnect (XMMS_OBJECT (val),
	                        XMMS_SIGNAL_CONFIG_VALUE_CHANGE, cb);
}

/**
  * Register a new configvalue. This should be called in the initcode
  * as XMMS2 won't allow set/get on values that hasn't been registered.
  *
  * @param path the path in the config tree.
  * @param default_value if the value was not found in the configfile, what should we use?
  * @param cb a callback function that will be called if the value is changed by the client. Could be set to NULL.
  * @param userdata data to the callback function.
  * @returns a newly allocated #xmms_config_value_t for the registered value.
  */

xmms_config_value_t *
xmms_config_value_register (const gchar *path, 
			    const gchar *default_value,
			    xmms_object_handler_t cb,
			    gpointer userdata)
{

	xmms_config_value_t *val;

	XMMS_DBG ("Registering: %s", path);

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
/** @} */
