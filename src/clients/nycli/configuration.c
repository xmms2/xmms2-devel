/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#include "configuration.h"

const gchar *const default_config =
"[main]\n\n"
"PROMPT=xmms2> \n"
"SERVER_AUTOSTART=true\n"
"AUTO_UNIQUE_COMPLETE=true\n"
"PLAYLIST_MARKER=->\n"
"GUESS_PLS=false\n"
"CLASSIC_LIST=true\n"
"CLASSIC_LIST_FORMAT=${artist} - ${title}\n"
"HISTORY_FILE=\n"
"STATUS_FORMAT=${playback_status}: ${artist} - ${title}: ${playtime} of ${duration}\n\n"
"[alias]\n\n"
"ls = list\n"
"clear = playlist clear\n"
"quit = exit\n"
"server kill = server shutdown\n"
"repeat = seek 0\n"
"mute = server volume 0\n"
"scap = stop ; playlist clear ; add $@ ; play\n"
"current = status -f $1\n"
"addpls = add -f -P $@\n";

/* Load a section from a keyfile to a hash-table
   (replace existing keys in the hash) */
static void
section_to_hash (GKeyFile *file, const gchar *section, GHashTable *hash)
{
	GError *error;
	gchar **keys;
	gint i;

	error = NULL;
	keys = g_key_file_get_keys (file, section, NULL, &error);
	if (error) {
		g_printf ("Error: Couldn't load configuration section %s!\n", section);
	}

	for (i = 0; keys[i] != NULL; i++) {
		gchar *uncompressed_value;

		uncompressed_value = g_key_file_get_value (file, section, keys[i],
		                                           NULL);
		g_hash_table_insert (hash,
		                     g_strdup (keys[i]),
		                     g_strcompress (uncompressed_value));
		g_free (uncompressed_value);
	}
	g_strfreev (keys);
}

configuration_t *
configuration_init (const gchar *path)
{
	configuration_t *config;
	gchar *history_file;

	config = g_new0 (configuration_t, 1);

	if (path == NULL) {
		char *dir;
		dir = g_new0 (char, XMMS_PATH_MAX);
		xmmsc_userconfdir_get (dir, XMMS_PATH_MAX);
		config->path = g_strdup_printf ("%s/clients/nycli.conf", dir);
		g_free (dir);
	} else {
		config->path = g_strdup (path);
	}

	/* init hash */
	config->values = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                        g_free, g_free);

	/* no aliases initially */
	config->aliases = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         g_free, g_free);

	if (!g_file_test (config->path, G_FILE_TEST_EXISTS)) {
		g_fprintf (stderr, "Creating %s...\n", config->path);

		if (!g_file_set_contents (config->path, default_config,
		                          strlen (default_config), NULL)) {
			g_fprintf (stderr, "Error: Can't create configuration file!\n");
		}
	}

	/* load the defaults */
	config->file = g_key_file_new ();
	g_key_file_load_from_data (config->file, default_config,
	                           strlen (default_config), G_KEY_FILE_NONE,
	                           NULL);
	section_to_hash (config->file, "main", config->values);
	g_key_file_free (config->file);

	config->file = g_key_file_new ();
	if (g_file_test (config->path, G_FILE_TEST_EXISTS)) {
		if (!g_key_file_load_from_file (config->file, config->path,
		                                G_KEY_FILE_NONE, NULL)) {
			g_printf ("Error: Couldn't load configuration file!\n");
		} else {
			/* load keys to hash table overriding default values */
			section_to_hash (config->file, "main", config->values);

			/* load aliases */
			section_to_hash (config->file, "alias", config->aliases);
		}
	}

	history_file = configuration_get_string (config, "HISTORY_FILE");
	if (!history_file || !*history_file) {
		gchar cfile[PATH_MAX];

		xmms_usercachedir_get (cfile, PATH_MAX);
		config->histpath = g_build_filename (cfile, HISTORY_FILE_BASE, NULL);
	} else {
		config->histpath = strdup (history_file);
	}

	return config;
}

void
configuration_free (configuration_t *config)
{
	g_free (config->path);
	g_free (config->histpath);
	g_key_file_free (config->file);
	g_hash_table_destroy (config->values);
	g_hash_table_destroy (config->aliases);
	g_free (config);
}

GHashTable *
configuration_get_aliases (configuration_t *config)
{
	return config->aliases;
}

gboolean
configuration_get_boolean (configuration_t *config, const gchar *key)
{
	gchar *val;

	if (!(val = g_hash_table_lookup (config->values, key))) {
		return FALSE;
	}

	if (!strcmp (val, "true") || !strcmp (val, "1")) {
		return TRUE;
	}

	return FALSE;
}

gchar *
configuration_get_string (configuration_t *config, const gchar *key)
{
	return g_hash_table_lookup (config->values, key);
}
