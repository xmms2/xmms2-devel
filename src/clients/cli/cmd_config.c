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
cmd_volume (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	gchar *end = NULL;
	guint vol;

	if (argc < 4) {
		print_error ("You must specify a channel and a volume level.");
	}

	vol = strtoul (argv[3], &end, 0);
	if (end == argv[3]) {
		print_error ("Please specify a channel and a number from 0-100.");
	}

	res = xmmsc_playback_volume_set (conn, argv[2], vol);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Failed to set volume.");
	}
	xmmsc_result_unref (res);
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
