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
"SHELL_START_MESSAGE=true\n"
"PLAYLIST_MARKER=->\n"
"GUESS_PLS=false\n"
"CLASSIC_LIST=true\n"
"CLASSIC_LIST_FORMAT=${artist} - ${title}\n"
"HISTORY_FILE=\n"
"STATUS_FORMAT=${playback_status}: ${artist} - ${title}: ${playtime} of ${duration}\n\n"
"[alias]\n\n"
"ls = list\n"
"clear = playlist clear\n"
"quit = server shutdown\n"
"repeat = seek 0\n"
"mute = server volume 0\n"
"scap = stop ; playlist clear ; add $@ ; play\n"
"status = current -r 1\n"
"addpls = add -f -P $@\n";

struct configuration_St {
	GHashTable *values;
	GHashTable *aliases;
};

static void
section_to_hash (GKeyFile *file, const gchar *section, GHashTable *hash)
{
	GError *error;
	gchar **keys;
	gint i;

	keys = g_key_file_get_keys (file, section, NULL, &error);

	for (i = 0; keys[i] != NULL; i++) {
		gchar *value;

		value = g_key_file_get_value (file, section, keys[i], NULL);
		g_hash_table_insert (hash, g_strdup (keys[i]), g_strcompress (value));
		g_free (value);
	}
	g_strfreev (keys);
}

static gboolean
configuration_create (const gchar *path)
{
	GError *error;
	gchar *dir;
	gint err;

	dir = g_path_get_dirname (path);
	err = g_mkdir_with_parents (dir, 0755);
	g_free (dir);

	if (err < 0) {
		g_fprintf (stderr, "Error: Can't create directory for configuration file '%s'. (%s)\n",
		           path, g_strerror (errno));
		return FALSE;
	}

	if (!g_file_set_contents (path, default_config, -1, &error)) {
		g_fprintf (stderr, "Error: Can't create configuration file '%s'. ('%s')\n",
		           path, error->message);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

/**
 * Merge one GKeyFile into another.
 */
static void
configuration_merge (GKeyFile *to, GKeyFile *from)
{
	gchar **groups, **keys, *value;
	gint i, j;

	groups = g_key_file_get_groups (from, NULL);

	for (i = 0; groups[i] != NULL; i++) {
		keys = g_key_file_get_keys (from, groups[i], NULL, NULL);
		for (j = 0; keys[j] != NULL; j++) {
			value = g_key_file_get_value (from, groups[i], keys[j], NULL);
			g_key_file_set_value (to, groups[i], keys[j], value);
			g_free (value);
		}
		g_strfreev (keys);
	}

	g_strfreev (groups);
}

/**
 * Load configuration.
 */
static GKeyFile *
configuration_load (const gchar *path)
{
	GKeyFile *keyfile, *custom;
	GError *error;

	keyfile = g_key_file_new ();
	custom = g_key_file_new ();

	/* load the defaults */
	g_key_file_load_from_data (keyfile, default_config, -1,
	                           G_KEY_FILE_NONE, NULL);

	/* create a default config file if missing */
	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		configuration_create (path);
	}

	/* try loading the config file */
	if (!g_key_file_load_from_file (custom, path, G_KEY_FILE_NONE, &error)) {
		g_printf ("Error: Couldn't load configuration file. (%s)\n",
		          error->message);
	} else {
		configuration_merge (keyfile, custom);
	}

	g_key_file_free (custom);

	return keyfile;
}

gchar *
configuration_get_filename (void)
{
	gchar *filename, *dir;

	dir = g_new0 (gchar, XMMS_PATH_MAX);
	xmmsc_userconfdir_get (dir, XMMS_PATH_MAX);
	filename = g_build_path (G_DIR_SEPARATOR_S, dir, "clients",
	                         "nycli.conf", NULL);
	g_free (dir);

	return filename;
}

configuration_t *
configuration_init (const gchar *filename)
{
	configuration_t *config;
	gchar *history_file;
	GKeyFile *keyfile;

	config = g_new0 (configuration_t, 1);

	/* init hash */
	config->values = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                        g_free, g_free);
	/* init aliases */
	config->aliases = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                         g_free, g_free);

	keyfile = configuration_load (filename);
	section_to_hash (keyfile, "main", config->values);
	section_to_hash (keyfile, "alias", config->aliases);
	g_key_file_free (keyfile);

	/* load history */
	history_file = configuration_get_string (config, "HISTORY_FILE");
	if (!*history_file) {
		gchar cfile[XMMS_PATH_MAX], *key, *value;

		xmms_usercachedir_get (cfile, XMMS_PATH_MAX);

		key = g_strdup ("HISTORY_FILE");
		value = g_build_filename (cfile, "nyxmms2_history", NULL);

		g_hash_table_replace (config->values, key, value);
	}

	return config;
}

void
configuration_free (configuration_t *config)
{
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
