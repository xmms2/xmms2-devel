/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
 *
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
#include "xmmspriv/xmms_utils.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_log.h"

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
	XMMS_CONFIG_STATE_PROPERTY
} xmms_configparser_state_t;

static GHashTable *xmms_config_listvalues (xmms_config_t *conf, xmms_error_t *err);
static xmms_config_property_t *xmms_config_property_new (const gchar *name);
static gchar *xmms_config_property_client_lookup (xmms_config_t *conf, gchar *key, xmms_error_t *err);
static gchar *xmms_config_property_client_register (xmms_config_t *config, const gchar *name, const gchar *def_value, xmms_error_t *error);

XMMS_CMD_DEFINE (setvalue, xmms_config_setvalue, xmms_config_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE (listvalues, xmms_config_listvalues, xmms_config_t *, DICT, NONE, NONE);
XMMS_CMD_DEFINE (getvalue, xmms_config_property_client_lookup, xmms_config_t *, STRING, STRING, NONE);
XMMS_CMD_DEFINE (regvalue, xmms_config_property_client_register, xmms_config_t *, STRING, STRING, STRING);

/**
 * @defgroup Config Config
 * @brief Controls configuration for the server.
 *
 * The configuration is saved to, and loaded from an XML file. It's split into
 * plugin, client and core parts. This documents the configuration for parts
 * inside the server. For plugin config see each server object's documentation.
 *
 * @ingroup XMMSServer
 * @{
 */

/**
 * Global parsed config
 */
struct xmms_config_St {
	xmms_object_t obj;

	GHashTable *properties;

	/* Lock on globals are great! */
	GMutex *mutex;

	/* parsing */
	gboolean is_parsing;
	GQueue *states;
	GQueue *sections;
	gchar *value_name;
	guint version;
};

/**
 * A config property in the configuration file
 */
struct xmms_config_property_St {
	xmms_object_t obj;

	/** Name of the config directive */
	const gchar *name;
	/** The data */
	gchar *value;
};


/**
 * Global config
 * Since there can only be one configuration per server
 * we can have the convenience of having it as a global variable.
 */

static xmms_config_t *global_config;

/**
 * Config file version
 */
#define XMMS_CONFIG_VERSION 2

/**
 * @}
 * @addtogroup Config
 * @{
 */

/**
 * Config functions
 */

/**
 * Lookup config key and return its associated value as a string.
 * This is a convenient function to make it easier to get a configuration value
 * rather than having to call #xmms_config_property_get_string separately.
 *
 * @param conf Global config
 * @param key Configuration property to lookup
 * @param err if error occurs this will be filled in
 *
 * @return A string with the value. If the value is an int it will return NULL
 */
const gchar *
xmms_config_property_lookup_get_string (xmms_config_t *conf, gchar *key,
                                        xmms_error_t *err)
{
	xmms_config_property_t *prop;

	prop = xmms_config_lookup (key);
	if (!prop) {
		xmms_error_set (err, XMMS_ERROR_NOENT,
		                "Trying to get non-existent property");
		return NULL;
	}

	return xmms_config_property_get_string (prop);
}

/**
 * Look up a config key from the global config
 * @param path A configuration path. Could be core.myconfig or
 * effect.foo.myconfig
 * @return An #xmms_config_property_t
 */
xmms_config_property_t *
xmms_config_lookup (const gchar *path)
{
	xmms_config_property_t *prop;
	g_return_val_if_fail (global_config, NULL);

	g_mutex_lock (global_config->mutex);
	prop = g_hash_table_lookup (global_config->properties, path);
	g_mutex_unlock (global_config->mutex);

	return prop;
}

/**
 * Get the name of a config property.
 * @param prop The config property
 * @return Name of config property
 */
const gchar *
xmms_config_property_get_name (const xmms_config_property_t *prop)
{
	g_return_val_if_fail (prop, NULL);

	return prop->name;
}

/**
 * Set the data of the config property to a new value
 * @param prop The config property
 * @param data The value to set
 */
void
xmms_config_property_set_data (xmms_config_property_t *prop, const gchar *data)
{
	GHashTable *dict;
	gchar *file;

	g_return_if_fail (prop);
	g_return_if_fail (data);

	/* check whether the value changed at all */
	if (prop->value && !strcmp (prop->value, data))
		return;

	g_free (prop->value);
	prop->value = g_strdup (data);
	xmms_object_emit (XMMS_OBJECT (prop),
	                  XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED,
	                  (gpointer) data);

	dict = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
	                              xmms_object_cmd_value_free);
	g_hash_table_insert (dict, (gchar *) prop->name,
	                     xmms_object_cmd_value_str_new (prop->value));

	xmms_object_emit_f (XMMS_OBJECT (global_config),
	                    XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED,
	                    XMMS_OBJECT_CMD_ARG_DICT,
	                    dict);

	g_hash_table_destroy (dict);

	/* save the database to disk, so we don't lose any data
	 * if the daemon crashes
	 */
	file = XMMS_BUILD_PATH ("xmms2.conf");
	xmms_config_save (file);
	g_free (file);
}

/**
 * Return the value of a config property as a string
 * @param prop The config property
 * @return value as string
 */
const gchar *
xmms_config_property_get_string (const xmms_config_property_t *prop)
{
	g_return_val_if_fail (prop, NULL);
	return prop->value;
}

/**
 * Return the value of a config property as an int
 * @param prop The config property
 * @return value as int
 */
gint
xmms_config_property_get_int (const xmms_config_property_t *prop)
{
	g_return_val_if_fail (prop, 0);
	if (prop->value)
		return atoi (prop->value);

	return 0;
}

/**
 * Return the value of a config property as a float
 * @param prop The config property
 * @return value as float
 */
gfloat
xmms_config_property_get_float (const xmms_config_property_t *prop)
{
	g_return_val_if_fail (prop, 0.0);
	if (prop->value)
		return atof (prop->value);

	return 0.0;
}

/**
 * Set a callback function for a config property.
 * This will be called each time the property's value changes.
 * @param prop The config property
 * @param cb The callback to set
 * @param userdata Data to pass on to the callback
 */
void
xmms_config_property_callback_set (xmms_config_property_t *prop,
                                   xmms_object_handler_t cb,
                                   gpointer userdata)
{
	g_return_if_fail (prop);

	if (!cb)
		return;

	xmms_object_connect (XMMS_OBJECT (prop),
	                     XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED,
	                     (xmms_object_handler_t) cb, userdata);
}

/**
 * Remove a callback from a config property
 * @param prop The config property
 * @param cb The callback to remove
 */
void
xmms_config_property_callback_remove (xmms_config_property_t *prop,
                                      xmms_object_handler_t cb,
                                      gpointer userdata)
{
	g_return_if_fail (prop);

	if (!cb)
		return;

	xmms_object_disconnect (XMMS_OBJECT (prop),
	                        XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED, cb, userdata);
}

/**
 * Register a new config property. This should be called from the init code
 * as XMMS2 won't allow set/get on properties that haven't been registered.
 *
 * @param path The path in the config tree.
 * @param default_value If the value was not found in the configfile, what
 * should we use?
 * @param cb A callback function that will be called if the value is changed by
 * the client. Can be set to NULL.
 * @param userdata Data to pass to the callback function.
 * @return A newly allocated #xmms_config_property_t for the registered
 * property.
 */
xmms_config_property_t *
xmms_config_property_register (const gchar *path,
                               const gchar *default_value,
                               xmms_object_handler_t cb,
                               gpointer userdata)
{

	xmms_config_property_t *prop;

	g_mutex_lock (global_config->mutex);

	prop = g_hash_table_lookup (global_config->properties, path);
	if (!prop) {
		prop = xmms_config_property_new (g_strdup (path));

		xmms_config_property_set_data (prop, (gchar *) default_value);
		g_hash_table_insert (global_config->properties,
		                     (gchar *) prop->name, prop);
	}

	if (cb) {
		xmms_config_property_callback_set (prop, cb, userdata);
	}

	g_mutex_unlock (global_config->mutex);

	return prop;
}

/**
 * @}
 *
 * @if internal
 * @addtogroup Config
 * @{
 */

/**
 * @internal Get the current parser state for the given element name
 * @param[in] name Element name to match to a state
 * @return Parser state matching element name
 */
static xmms_configparser_state_t
get_current_state (const gchar *name)
{
	static struct {
		const gchar *name;
		xmms_configparser_state_t state;
	} *ptr, lookup[] = {
		{"xmms", XMMS_CONFIG_STATE_START},
		{"section", XMMS_CONFIG_STATE_SECTION},
		{"property", XMMS_CONFIG_STATE_PROPERTY},
		{NULL, XMMS_CONFIG_STATE_INVALID}
	};

	for (ptr = lookup; ptr && ptr->name; ptr++) {
		if (!strcmp (ptr->name, name)) {
			return ptr->state;
		}
	}

	return XMMS_CONFIG_STATE_INVALID;
}

/**
 * @internal Look for the value associated with an attribute name, given lists
 * of attribute names and attribute values.
 * @param[in] names List of attribute names
 * @param[in] values List of attribute values matching up to names
 * @param[in] needle Attribute name to look for
 * @return The attribute value, or NULL if not found
 */
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

/**
 * @internal Parse start tag in config file. This function is called whenever
 * a start tag is encountered by the GMarkupParser from #xmms_config_init
 * @param ctx The parser context.
 * @param name The name of the element encountered
 * @param attr_name List of attribute names in tag
 * @param attr_data List of attribute data in tag
 * @param userdata User data - In this case, the global config
 * @param error GError to be filled in if an error is encountered
 */
static void
xmms_config_parse_start (GMarkupParseContext *ctx,
                         const gchar *name,
                         const gchar **attr_name,
                         const gchar **attr_data,
                         gpointer userdata,
                         GError **error)
{
	xmms_config_t *config = userdata;
	xmms_configparser_state_t state;
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
			/* check config version here */
			attr = lookup_attribute (attr_name, attr_data, "version");
			if (attr) {
				if (strcmp (attr, "0.02") == 0) {
					config->version = 2;
				} else {
					config->version = atoi (attr);
				}
			}
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
		case XMMS_CONFIG_STATE_PROPERTY:
			g_free (config->value_name);
			config->value_name = g_strdup (attr);

			break;
		default:
			break;
	}
}

/**
 * @internal Parse end tag in config file. This function is called whenever
 * an end tag is encountered by the GMarkupParser from #xmms_config_init
 * @param ctx The parser context.
 * @param name The name of the element encountered
 * @param userdata User data - In this case, the global config
 * @param error GError to be filled in if an error is encountered
 */
static void
xmms_config_parse_end (GMarkupParseContext *ctx,
                       const gchar *name,
                       gpointer userdata,
                       GError **error)
{
	xmms_config_t *config = userdata;
	xmms_configparser_state_t state;

	state = GPOINTER_TO_INT (g_queue_pop_head (config->states));

	switch (state) {
		case XMMS_CONFIG_STATE_SECTION:
			g_free (g_queue_pop_head (config->sections));

			break;
		case XMMS_CONFIG_STATE_PROPERTY:
			g_free (config->value_name);
			config->value_name = NULL;

			break;
		default:
			break;
	}
}

/**
 * @internal Parse text in config file. This function is called whenever
 * text (anything between start and end tags) is encountered by the
 * GMarkupParser from #xmms_config_init
 * @param ctx The parser context.
 * @param text The text
 * @param text_len Length of the text
 * @param userdata User data - In this case, the global config
 * @param error GError to be filled in if an error is encountered
 */
static void
xmms_config_parse_text (GMarkupParseContext *ctx,
                        const gchar *text,
                        gsize text_len,
                        gpointer userdata,
                        GError **error)
{
	xmms_config_t *config = userdata;
	xmms_configparser_state_t state;
	xmms_config_property_t *prop;
	GList *l;
	gchar key[256] = "";
	gsize siz = sizeof (key);

	state = GPOINTER_TO_INT (g_queue_peek_head (config->states));

	if (state != XMMS_CONFIG_STATE_PROPERTY)
		return;

	/* assemble the config key, based on the traversed sections */
	for (l = config->sections->tail; l; l = l->prev) {
		g_strlcat (key, l->data, siz);
		g_strlcat (key, ".", siz);
	}

	g_strlcat (key, config->value_name, siz);

	prop = xmms_config_property_new (g_strdup (key));
	xmms_config_property_set_data (prop, (gchar *) text);

	g_hash_table_insert (config->properties, (gchar *) prop->name, prop);
}

/**
 * @internal Set a key to a new value
 * @param conf The config
 * @param key The key to look for
 * @param value The value to set the key to
 * @param err To be filled in if an error occurs
 */
void
xmms_config_setvalue (xmms_config_t *conf, const gchar *key, const gchar *value,
                      xmms_error_t *err)
{
	xmms_config_property_t *prop;

	prop = xmms_config_lookup (key);
	if (prop) {
		xmms_config_property_set_data (prop, value);
	} else {
		xmms_error_set (err, XMMS_ERROR_NOENT,
		                "Trying to set non-existent config property");
	}

}

/**
 * @internal Convert global config properties dict to a normal dict
 * @param key The dict key
 * @param property An xmms_config_property_t
 * @param udata The dict to store configvals
 */
static void
xmms_config_foreach_dict (gpointer key, xmms_config_property_t *prop,
                          GHashTable *dict)
{
	g_hash_table_insert (dict, g_strdup (key),
	                     xmms_object_cmd_value_str_new (prop->value));
}

/**
 * @internal List all keys and values in the config.
 * @param conf The config
 * @param err To be filled in if an error occurs
 * @return a dict with config properties and values
 */
static GHashTable *
xmms_config_listvalues (xmms_config_t *conf, xmms_error_t *err)
{
	GHashTable *ret;

	ret = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
	                             xmms_object_cmd_value_free);

	g_mutex_lock (conf->mutex);
	g_hash_table_foreach (conf->properties,
	                      (GHFunc) xmms_config_foreach_dict,
	                      (gpointer) ret);
	g_mutex_unlock (conf->mutex);

	return ret;
}

/**
 * @internal Look for a key in the config and return its value as a string
 * @param conf The config
 * @param key The key to look for
 * @param err To be filled in if an error occurs
 * @return The value of the key, or NULL if not found
 */
static gchar *
xmms_config_property_client_lookup (xmms_config_t *conf, gchar *key,
                                 xmms_error_t *err)
{
	return g_strdup (xmms_config_property_lookup_get_string (conf, key, err));
}

/**
 * @internal Destroy a config object
 * @param object The object to destroy
 */
static void
xmms_config_destroy (xmms_object_t *object)
{
	xmms_config_t *config = (xmms_config_t *)object;

	g_mutex_free (config->mutex);

	g_hash_table_destroy (config->properties);

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_CONFIG);
}

static gboolean
foreach_remove (gpointer key, gpointer value, gpointer udata)
{
	return TRUE; /* remove this key/value pair */
}

/**
 * @internal Clear data in a config object
 * @param config The config object to clear
 */
static void
clear_config (xmms_config_t *config)
{
	g_hash_table_foreach_remove (config->properties, foreach_remove, NULL);

	config->version = XMMS_CONFIG_VERSION;

	g_free (config->value_name);
	config->value_name = NULL;
}

/**
 * @internal Initialize and parse the config file. Resets to default config
 * on parse error.
 * @param[in] filename The absolute path to a config file as a string.
 */
void
xmms_config_init (const gchar *filename)
{
	GMarkupParser pars;
	GMarkupParseContext *ctx;
	xmms_config_t *config;
	int ret, fd = -1;
	gboolean parserr = FALSE, eof = FALSE;

	config = xmms_object_new (xmms_config_t, xmms_config_destroy);
	config->mutex = g_mutex_new ();

	config->properties = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                            g_free,
	                                            (GDestroyNotify) __int_xmms_object_unref);
	config->version = 0;
	global_config = config;

	xmms_ipc_object_register (XMMS_IPC_OBJECT_CONFIG, XMMS_OBJECT (config));
	xmms_ipc_broadcast_register (XMMS_OBJECT (config),
	                             XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED);

	memset (&pars, 0, sizeof (pars));

	pars.start_element = xmms_config_parse_start;
	pars.end_element = xmms_config_parse_end;
	pars.text = xmms_config_parse_text;

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		fd = open (filename, O_RDONLY);
	}

	if (fd > -1) {
		config->is_parsing = TRUE;
		config->states = g_queue_new ();
		config->sections = g_queue_new ();
		ctx = g_markup_parse_context_new (&pars, 0, config, NULL);

		while ((!eof) && (!parserr)) {
			GError *error = NULL;
			gchar buffer[1024];

			ret = read (fd, buffer, 1024);
			if (ret < 1) {
				g_markup_parse_context_end_parse (ctx, &error);
				if (error) {
					xmms_log_error ("Cannot parse config file: %s",
					                error->message);
					g_error_free (error);
					error = NULL;
					parserr = TRUE;
				}
				eof = TRUE;
			}

			g_markup_parse_context_parse (ctx, buffer, ret, &error);
			if (error) {
				xmms_log_error ("Cannot parse config file: %s",
				                error->message);
				g_error_free (error);
				error = NULL;
				parserr = TRUE;
			}
			/* check config file version, assumes that g_markup_context_parse
			 * above managed to parse the <xmms> element during the first
			 * iteration of this loop */
			if (XMMS_CONFIG_VERSION > config->version) {
				clear_config (config);
				break;
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
	} else {
		xmms_log_info ("No configfile specified, using default values.");
	}

	if (parserr) {
		xmms_log_info ("The config file could not be parsed, reverting to default configuration..");
		clear_config (config);
	}

	xmms_object_cmd_add (XMMS_OBJECT (config),
	                     XMMS_IPC_CMD_SETVALUE,
	                     XMMS_CMD_FUNC (setvalue));
	xmms_object_cmd_add (XMMS_OBJECT (config),
	                     XMMS_IPC_CMD_GETVALUE,
	                     XMMS_CMD_FUNC (getvalue));
	xmms_object_cmd_add (XMMS_OBJECT (config),
	                     XMMS_IPC_CMD_LISTVALUES,
	                     XMMS_CMD_FUNC (listvalues));
	xmms_object_cmd_add (XMMS_OBJECT (config),
	                     XMMS_IPC_CMD_REGVALUE,
	                     XMMS_CMD_FUNC (regvalue));
}

/**
 * @internal Shut down the config layer - free memory from the global
 * configuration.
 */
void
xmms_config_shutdown ()
{
	xmms_object_unref (global_config);

}

/**
 * @internal Append new data to a GNode tree
 * @param parent The parent node
 * @param new_data New data
 * @return The new GNode
 */
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
			 * note that this never happens for "property" nodes, so
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

/**
 * @internal Function to help build a tree representing config data to be saved.
 * @param key The key to add to the tree
 * @param prop The property associated to key
 * @param udata User data - in this case, the GNode tree we're modifying
 */
static void
add_to_tree_foreach (gpointer key, gpointer prop, gpointer udata)
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
	data[1] = g_strdup (xmms_config_property_get_string (prop));

	g_strfreev (subkey);
}

/**
 * @internal Write data in a GNode to file.
 * @param node The GNode to dump
 * @param fp An open file stream
 */
static void
dump_node (GNode *node, FILE *fp)
{
	gchar **data = node->data;
	gboolean is_root = G_NODE_IS_ROOT (node);
	static gchar indent[128] = "";
	static gsize siz = sizeof (indent);
	gsize len;

	if (G_NODE_IS_LEAF (node) && !is_root) {
		fprintf (fp, "%s<property name=\"%s\">%s</property>\n",
		         indent, data[0], data[1]);
	} else {
		if (is_root) {
			fprintf (fp, "<?xml version=\"1.0\"?>\n<%s version=\"%i\">\n",
			         (gchar *) node->data, XMMS_CONFIG_VERSION);
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
 * @internal Save the global configuration to disk.
 * @param file Absolute path to configfile. This will be overwritten.
 * @return TRUE on success.
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

	tree = g_node_new ((gpointer) "xmms");
	g_hash_table_foreach (global_config->properties, add_to_tree_foreach, tree);

	dump_node (tree, fp);

	g_node_destroy (tree);
	fclose (fp);

	return TRUE;
}

/*
 * Value manipulation
 */

/**
 * @internal Destroy a config value
 * @param object The object to destroy
 */
static void
xmms_config_property_destroy (xmms_object_t *object)
{
	xmms_config_property_t *prop = (xmms_config_property_t *) object;

	/* don't free val->name here, it's taken care of in
	 * xmms_config_destroy()
	 */
	g_free (prop->value);
}

/**
 * @internal Create a new config value
 * @param name The name of the new config value
 */
static xmms_config_property_t *
xmms_config_property_new (const gchar *name)
{
	xmms_config_property_t *ret;

	ret = xmms_object_new (xmms_config_property_t, xmms_config_property_destroy);
	ret->name = name;

	return ret;
}

/**
 * @internal Register a client config value
 * @param config The config
 * @param name The name of the config value
 * @param def_value The default value to use
 * @param error To be filled in if an error occurs
 * @return The full path to the config value registered
 */
static gchar *
xmms_config_property_client_register (xmms_config_t *config,
                                      const gchar *name,
                                      const gchar *def_value,
                                      xmms_error_t *error)
{
	gchar *tmp;
	tmp = g_strdup_printf ("clients.%s", name);
	xmms_config_property_register (tmp, def_value, NULL, NULL);
	return tmp;
}

/** @} */
