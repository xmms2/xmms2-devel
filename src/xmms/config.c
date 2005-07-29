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

#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmspriv/xmms_config.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_defs.h"

/*
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/signal_xmms.h"
#include "xmms/plugin.h"
#include "xmms/ipc.h"
*/

/** @internal */
typedef enum {
	XMMS_CONFIG_STATE_INVALID,
	XMMS_CONFIG_STATE_START,
	XMMS_CONFIG_STATE_SECTION,
	XMMS_CONFIG_STATE_VALUE
} xmms_config_state_t;

static GList *xmms_config_listvalues (xmms_config_t *conf, xmms_error_t *err);
static xmms_config_value_t *xmms_config_value_new (const gchar *name);
static gchar *xmms_config_value_client_lookup (xmms_config_t *conf, gchar *key, xmms_error_t *err);
static gchar *xmms_config_value_client_register (xmms_config_t *config, const gchar *value, const gchar *def_value, xmms_error_t *error);

XMMS_CMD_DEFINE (setvalue, xmms_config_setvalue, xmms_config_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE (listvalues, xmms_config_listvalues, xmms_config_t *, LIST, NONE, NONE);
XMMS_CMD_DEFINE (getvalue, xmms_config_value_client_lookup, xmms_config_t *, STRING, STRING, NONE);
XMMS_CMD_DEFINE (regvalue, xmms_config_value_client_register, xmms_config_t *, STRING, STRING, STRING);

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

/**
 * Global parsed config
 */
struct xmms_config_St {
	xmms_object_t obj;

	GHashTable *values;

	/* Lock on globals are great! */
	GMutex *mutex;

	/* parsing */
	gboolean is_parsing;
	GQueue *states;
	GQueue *sections;
	gchar *value_name;
};

/**
 * A value in the configuration file
 */
struct xmms_config_value_St {
	xmms_object_t obj;

	/** Name of the config directive */
	const gchar *name;
	/** The data */
	gchar *data;
};


/**
  * Global config
  * Since there can only be one configuration per server
  * we can have the convineance to have it global.
  */

xmms_config_t *global_config;

/*
 * Config functions
 */

static void
add_to_list_foreach (gpointer key, gpointer value, gpointer udata)
{
	xmms_object_cmd_value_t *val;
	GList **list = udata;

	val = xmms_object_cmd_value_str_new (g_strdup ((gchar *)key));

	*list = g_list_prepend (*list, val);
}

static xmms_config_state_t
get_current_state (const gchar *name)
{
	static struct {
		gchar *name;
		xmms_config_state_t state;
	} *ptr, lookup[] = {
		{"xmms", XMMS_CONFIG_STATE_START},
		{"section", XMMS_CONFIG_STATE_SECTION},
		{"value", XMMS_CONFIG_STATE_VALUE},
		{NULL, XMMS_CONFIG_STATE_INVALID}
	};

	for (ptr = lookup; ptr && ptr->name; ptr++) {
		if (!strcmp (ptr->name, name)) {
			return ptr->state;
		}
	}

	return XMMS_CONFIG_STATE_INVALID;
}

static const gchar *
lookup_attribute (const gchar **names, const gchar **values,
                  const gchar *needle)
{
	const gchar **n, **v;

	for (n = names, v = values; *n && *v; n++, v++) {
		if (!strcmp ((gchar *) *n, needle)) {
			return *v;
		}
	}

	return NULL;
}

static void
xmms_config_parse_start (GMarkupParseContext *ctx,
                         const gchar *name,
                         const gchar **attr_name,
                         const gchar **attr_data,
                         gpointer userdata,
                         GError **error)
{
	xmms_config_t *config = userdata;
	xmms_config_state_t state;
	const gchar *attr;

	state = get_current_state (name);
	g_queue_push_head (config->states, GINT_TO_POINTER (state));

	switch (state) {
		case XMMS_CONFIG_STATE_INVALID:
			*error = g_error_new (G_MARKUP_ERROR,
			                      G_MARKUP_ERROR_UNKNOWN_ELEMENT,
			                      "Unknown element '%s'", name);
			return;
		case XMMS_CONFIG_STATE_START:
			/* nothing to be done here */
			return;
		default:
			break;
	}

	attr = lookup_attribute (attr_name, attr_data, "name");
	if (!attr) {
		*error = g_error_new (G_MARKUP_ERROR,
		                      G_MARKUP_ERROR_INVALID_CONTENT,
		                      "Attribute 'name' missing");
		return;
	}

	switch (state) {
		case XMMS_CONFIG_STATE_SECTION:
			g_queue_push_head (config->sections, g_strdup (attr));

			break;
		case XMMS_CONFIG_STATE_VALUE:
			g_free (config->value_name);
			config->value_name = g_strdup (attr);

			break;
		default:
			break;
	}
}

static void
xmms_config_parse_end (GMarkupParseContext *ctx,
                       const gchar *name,
                       gpointer userdata,
                       GError **error)
{
	xmms_config_t *config = userdata;
	xmms_config_state_t state;

	state = GPOINTER_TO_INT (g_queue_pop_head (config->states));

	switch (state) {
		case XMMS_CONFIG_STATE_SECTION:
			g_free (g_queue_pop_head (config->sections));

			break;
		case XMMS_CONFIG_STATE_VALUE:
			g_free (config->value_name);
			config->value_name = NULL;

			break;
		default:
			break;
	}
}

static void
xmms_config_parse_text (GMarkupParseContext *ctx,
                        const gchar *text,
                        gsize text_len,
                        gpointer userdata,
                        GError **error)
{
	xmms_config_t *config = userdata;
	xmms_config_state_t state;
	xmms_config_value_t *val;
	GList *l;
	gchar key[256] = "";
	gsize siz = sizeof (key);

	state = GPOINTER_TO_INT (g_queue_peek_head (config->states));

	if (state != XMMS_CONFIG_STATE_VALUE)
		return;

	/* assemble the config key, based on the traversed sections */
	for (l = config->sections->tail; l; l = l->prev) {
		g_strlcat (key, l->data, siz);
		g_strlcat (key, ".", siz);
	}

	g_strlcat (key, config->value_name, siz);

	val = xmms_config_value_new (g_strdup (key));
	xmms_config_value_data_set (val, (gchar *) text);

	g_hash_table_insert (config->values, (gchar *) val->name, val);
}



/**
 * Sets a key to a new value
 */

void
xmms_config_setvalue (xmms_config_t *conf, gchar *key, const gchar *value, xmms_error_t *err)
{
	xmms_config_value_t *val;

	val = xmms_config_lookup (key);
	if (val) {
		xmms_config_value_data_set (val, value);
	} else {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Trying to set nonexistant configvalue");
	}

}

static gint
cb_sort_config_list (const xmms_object_cmd_value_t *v1,
                     const xmms_object_cmd_value_t *v2)
{
	return g_strcasecmp (v1->value.string, v2->value.string);
}

/**
  * List all keys and values in the list.
  * @returns a GList with strings with format "key=value"
  */
static GList *
xmms_config_listvalues (xmms_config_t *conf, xmms_error_t *err)
{
	GList *ret = NULL;

	g_mutex_lock (conf->mutex);
	
	g_hash_table_foreach (conf->values, add_to_list_foreach, &ret);

	g_mutex_unlock (conf->mutex);

	ret = g_list_sort (ret, (GCompareFunc) cb_sort_config_list);

	return ret;
}


/**
  * Lookup configvalue and return it as a string.
  * This is a convinient function for removing the step
  * with #xmms_config_value_string_get
  *
  * @param conf Global config
  * @param key Configuration value to lookup
  * @param err if error occurs this will be filled in
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

static gchar *
xmms_config_value_client_lookup (xmms_config_t *conf, gchar *key, xmms_error_t *err)
{
	return g_strdup (xmms_config_value_lookup_string_get (conf, key, err));
}


static void
xmms_config_destroy (xmms_object_t *object) 
{
	xmms_config_t *config = (xmms_config_t *)object;

	g_mutex_free (config->mutex);

	g_hash_table_destroy (config->values);

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_CONFIG);
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
	int ret, fd = -1;

	config = xmms_object_new (xmms_config_t, xmms_config_destroy);
	config->mutex = g_mutex_new ();

	config->values = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                        g_free,
	                                        (GDestroyNotify) __int_xmms_object_unref);
	global_config = config;

	memset (&pars, 0, sizeof (pars));

	pars.start_element = xmms_config_parse_start;
	pars.end_element = xmms_config_parse_end;
	pars.text = xmms_config_parse_text;

	if (filename)
		fd = open (filename, O_RDONLY);

	if (fd > -1) {
		config->is_parsing = TRUE;
		config->states = g_queue_new ();
		config->sections = g_queue_new ();
		ctx = g_markup_parse_context_new (&pars, 0, config, NULL);

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

		while (!g_queue_is_empty (config->sections)) {
			g_free (g_queue_pop_head (config->sections));
		}

		g_queue_free (config->states);
		g_queue_free (config->sections);

		config->is_parsing = FALSE;
	}

	xmms_object_cmd_add (XMMS_OBJECT (config), XMMS_IPC_CMD_SETVALUE, XMMS_CMD_FUNC (setvalue));
	xmms_object_cmd_add (XMMS_OBJECT (config), XMMS_IPC_CMD_GETVALUE, XMMS_CMD_FUNC (getvalue));
	xmms_object_cmd_add (XMMS_OBJECT (config), XMMS_IPC_CMD_LISTVALUES, XMMS_CMD_FUNC (listvalues));
	xmms_object_cmd_add (XMMS_OBJECT (config), XMMS_IPC_CMD_REGVALUE, XMMS_CMD_FUNC (regvalue));
	xmms_ipc_object_register (XMMS_IPC_OBJECT_CONFIG, XMMS_OBJECT (config));
	xmms_ipc_broadcast_register (XMMS_OBJECT (config),
	                             XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED);

	return TRUE;
}

/**
 * Free memory from the global configuration
 */
void
xmms_config_shutdown ()
{
	xmms_object_unref (global_config);
	
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
	gpointer *data = NULL;

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
dump_node (GNode *node, FILE *fp)
{
	gchar **data = node->data;
	gboolean is_root = G_NODE_IS_ROOT (node);
	static gchar indent[128] = "";
	static gsize siz = sizeof (indent);
	gsize len;

	if (G_NODE_IS_LEAF (node) && !is_root) {
		fprintf (fp, "%s<value name=\"%s\">%s</value>\n",
		         indent, data[0], data[1]);
	} else {
		if (is_root) {
			fprintf (fp, "<?xml version=\"1.0\"?>\n<%s>\n",
			         (gchar *) node->data);
		} else {
			fprintf (fp, "%s<section name=\"%s\">\n",
			         indent, data[0]);
		}

		len = g_strlcat (indent, "\t", siz);
		g_node_children_foreach (node, G_TRAVERSE_ALL,
		                         (GNodeForeachFunc) dump_node, fp);
		indent[len - 1] = '\0';

		if (is_root) {
			fprintf (fp, "</%s>\n", (gchar *) node->data);
		} else {
			fprintf (fp, "%s</section>\n", indent);
		}
	}

	if (!is_root) {
		g_free (data[0]);
		g_free (data[1]);
		g_free (data);
	}
}

/**
  * Save the global configuration to disk.
  * @param file absolute path to configfile. This will be overwritten.
  * @returns #TRUE on success.
  */

gboolean
xmms_config_save (const gchar *file)
{
	GNode *tree = NULL;
	FILE *fp = NULL;

	g_return_val_if_fail (global_config, FALSE);
	g_return_val_if_fail (file, FALSE);

	/* don't try to save config while it's being read */
	if (global_config->is_parsing)
		return FALSE;

	if (!(fp = fopen (file, "w"))) {
		xmms_log_error ("Couldn't open %s for writing.", file);
		return FALSE;
	}

	tree = g_node_new ("xmms");
	g_hash_table_foreach (global_config->values, add_to_tree_foreach, tree);

	dump_node (tree, fp);

	g_node_destroy (tree);
	fclose (fp);

	return TRUE;
}

/*
 * Value manipulation 
 */

static void
xmms_config_value_destroy (xmms_object_t *object)
{
	xmms_config_value_t *val = (xmms_config_value_t *) object;

	/* don't free val->name here, it's taken care of in
	 * xmms_config_destroy()
	 */
	g_free (val->data);
}

static xmms_config_value_t *
xmms_config_value_new (const gchar *name)
{
	xmms_config_value_t *ret;

	ret = xmms_object_new (xmms_config_value_t, xmms_config_value_destroy);
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
xmms_config_value_data_set (xmms_config_value_t *val, const gchar *data)
{
	GHashTable *dict;
	gchar file[XMMS_PATH_MAX];

	g_return_if_fail (val);
	g_return_if_fail (data);

	/* check whether the value changed at all */
	if (val->data && !strcmp (val->data, data))
		return;

	g_free (val->data);
	val->data = g_strdup (data);
	xmms_object_emit (XMMS_OBJECT (val), XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED,
			  (gpointer) data);

	dict = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, xmms_object_cmd_value_free);
	g_hash_table_insert (dict, "name", xmms_object_cmd_value_str_new (g_strdup (val->name)));
	g_hash_table_insert (dict, "value", xmms_object_cmd_value_str_new (g_strdup (val->data)));
	
	xmms_object_emit_f (XMMS_OBJECT (global_config),
			    XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED,
	                    XMMS_OBJECT_CMD_ARG_DICT,
	                    dict);

	g_hash_table_destroy (dict);

	/* save the database to disk, so we don't loose any data
	 * if the daemon crashes
	 */
	g_snprintf (file, sizeof (file), "%s/.xmms2/xmms2.conf",
	            g_get_home_dir ());

	xmms_config_save (file);
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

/**
 * Set a callback function for the value.
 * This will be called each time the value changes.
 */
void
xmms_config_value_callback_set (xmms_config_value_t *val,
				xmms_object_handler_t cb,
				gpointer userdata)
{
	g_return_if_fail (val);

	if (!cb)
		return;

	xmms_object_connect (XMMS_OBJECT (val), 
			     XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED, 
			     (xmms_object_handler_t) cb, userdata);
}

/**
 * Remove a callback from the value 
 */
void
xmms_config_value_callback_remove (xmms_config_value_t *val,
                                   xmms_object_handler_t cb)
{
	g_return_if_fail (val);

	if (!cb)
		return;

	xmms_object_disconnect (XMMS_OBJECT (val),
	                        XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED, cb);
}

static gchar *
xmms_config_value_client_register (xmms_config_t *config,
				   const gchar *value,
				   const gchar *def_value,
				   xmms_error_t *error)
{
	gchar *tmp;
	tmp = g_strdup_printf ("clients.%s", value);
	xmms_config_value_register (tmp, def_value, NULL, NULL);
	return tmp;
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

	g_mutex_lock (global_config->mutex);

	val = g_hash_table_lookup (global_config->values, path);
	if (!val) {
		gchar *name = strrchr (path, '.');

		/* get our own copy of the string */
		path = g_strdup (path);

		if (!name) 
			val = xmms_config_value_new (path);
		else
			val = xmms_config_value_new (name+1);

		xmms_config_value_data_set (val, (gchar *) default_value);
		g_hash_table_insert (global_config->values, (gchar *) path, val);
	}

	if (cb) 
		xmms_config_value_callback_set (val, cb, userdata);

	g_mutex_unlock (global_config->mutex);

	return val;
}
/** @} */
