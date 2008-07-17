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
 */

#include "configuration.h"

#define KEY_VALUE_COPY(k,v) g_strdup ((k)), g_strdup ((v))

static GHashTable *
init_hash ()
{
	GHashTable *table;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	g_hash_table_insert (table, KEY_VALUE_COPY("PROMPT", "nycli> "));
	g_hash_table_insert (table, KEY_VALUE_COPY("AUTO_UNIQUE_COMPLETE", "true"));
	g_hash_table_insert (table, KEY_VALUE_COPY("PLAYLIST_MARKER", "->"));

	return table;
}

configuration_t *
configuration_init (const gchar *path)
{
	configuration_t *config;
	gchar **keys;
	gint i;

	GError *error;

	config = g_new0 (configuration_t, 1);

	if (path == NULL) {
		char *dir;
		dir = g_new0 (char, PATH_MAX);
		xmmsc_userconfdir_get (dir, PATH_MAX);
		config->path = g_strdup_printf ("%s/nycli.conf", dir);
		g_free (dir);
	} else {
		config->path = g_strdup (path);
	}

	/* init hash to default values */
	config->values = init_hash ();

	config->file = g_key_file_new ();
	if (g_file_test (config->path, G_FILE_TEST_EXISTS)) {
		if (!g_key_file_load_from_file (config->file, config->path,
		                                G_KEY_FILE_KEEP_COMMENTS, NULL)) {
			g_printf ("Error: Couldn't load configuration file");
		} else {
			/* load keys to hash table overriding default values */
			error = NULL;
			keys = g_key_file_get_keys (config->file, "main", NULL, &error);
			if (error) {
				g_printf ("Error: Couldn't load configuration parameters");
			}
			
			for (i = 0; keys[i] != NULL; i++) {
				g_hash_table_insert (config->values,
				                     g_strdup (keys[i]),
				                     g_key_file_get_value (config->file,
							                               "main", keys[i],
				                                           NULL));
			}
			g_strfreev (keys);
		}
	}
	
	return config;
}

void
configuration_free (configuration_t *config)
{
	g_free (config->path);
	g_key_file_free (config->file);
	g_hash_table_destroy (config->values);
}

gboolean
configuration_get_boolean (configuration_t *config, gchar *key)
{
	gchar *val;

	val = g_hash_table_lookup (config->values, key);
	
	if (!strcmp (val, "true") || !strcmp (val, "1")) {
		return TRUE;
	}
	
	return FALSE;
}

gchar *
configuration_get_string (configuration_t *config, gchar *key)
{
	return g_hash_table_lookup (config->values, key);
}
