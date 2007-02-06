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

#include "cmd_config.h"
#include "common.h"

void
cmd_config (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *key, *value;

	if (argc < 4) {
		print_error ("You need to specify a configkey and a value");
	}

	key = argv[2];

	if (g_strcasecmp(argv[3], "=") == 0) {
		value = argv[4];
	} else {
		value = argv[3];
	}

	if (!value) {
		print_error ("You need to specify a configkey and a value");
	}

	res = xmmsc_configval_set (conn, key, value);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't set config value: %s",
		             xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);

	print_info ("Config value %s set to %s", key, value);
}

/** TODO: doesn't show parameters **/

void
cmd_config_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_configval_list (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_dict_foreach (res, print_hash, NULL);
	xmmsc_result_unref (res);
}

void
get_keys (const void *key, xmmsc_result_value_type_t type, const void *value, void *user_data)
{
	GList **l = user_data;
	g_return_if_fail (l);

	*l = g_list_prepend (*l, g_strdup ((gchar *)key));
}

void
cmd_volume (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	int i;
	GList *channels, *cur;
	gchar *end = NULL;
	guint vol;

	if (argc < 3) {
		print_error ("You must specify a volume level.");
	}

	channels = NULL;
	for (i = 2; i < argc - 1; i++) {
		channels = g_list_prepend (channels, argv[i]);
	}

	vol = strtoul (argv[argc - 1], &end, 0);
	if (end == argv[argc - 1]) {
		print_error ("Please specify a number from 0-100.");
	}

	if (!channels) {
		res = xmmsc_playback_volume_get (conn);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("Failed to get channel information");
		}

		xmmsc_result_dict_foreach (res, get_keys, &channels);

		xmmsc_result_unref (res);
	}

	for (cur = channels; cur; cur = g_list_next (cur)) {
		res = xmmsc_playback_volume_set (conn, cur->data, vol);
		xmmsc_result_wait (res);

		if (xmmsc_result_iserror (res)) {
			print_error ("Failed to set volume.");
		}
		xmmsc_result_unref (res);
	}

	g_list_free (channels);
}


void
cmd_volume_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_playback_volume_get (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Failed to get volume.");
	}
	xmmsc_result_dict_foreach (res, print_hash, NULL);

	xmmsc_result_unref (res);
}
