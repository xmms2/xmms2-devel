/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

typedef struct volume_channel_St {
	const gchar *name;
	gint volume;
} volume_channel_t;

void
cmd_config (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gchar *key;
	const gchar *value;

	if (argc < 3) {
		print_error ("You need to specify at least a configkey");
	}

	key = argv[2];

	if (argc == 3) {
		res = xmmsc_configval_get (conn, key);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);

		if (xmmsv_get_error (val, &errmsg)) {
			print_error ("Couldn't get config value: %s", errmsg);
		}

		xmmsv_get_string (val, &value);
		print_info ("%s", value);

		xmmsc_result_unref (res);

		return;
	}

	if (g_ascii_strcasecmp (argv[3], "=") == 0) {
		value = argv[4];
	} else {
		value = argv[3];
	}

	if (!value) {
		print_error ("You need to specify a configkey and a value");
	}

	res = xmmsc_configval_set (conn, key, value);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Couldn't set config value: %s", errmsg);
	}
	xmmsc_result_unref (res);

	print_info ("Config value %s set to %s", key, value);
}

/** TODO: doesn't show parameters **/

void
cmd_config_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	res = xmmsc_configval_list (conn);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("%s", errmsg);
	}

	xmmsv_dict_foreach (val, print_hash, NULL);
	xmmsc_result_unref (res);
}

void
get_keys (const gchar *key, xmmsv_t *value, void *user_data)
{
	GList **l = user_data;
	volume_channel_t *chan;

	g_return_if_fail (l);

	chan = g_new (volume_channel_t, 1);
	chan->name = g_strdup ((const gchar *)key);
	xmmsv_get_int (value, &chan->volume);

	*l = g_list_prepend (*l, chan);
}

guint
volume_get (xmmsc_connection_t *conn, const gchar *name)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	gint ret;

	res = xmmsc_playback_volume_get (conn);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Failed to get volume: %s", errmsg);
	}

	if (!xmmsv_dict_entry_get_int (val, name, &ret)) {
		ret = 0;
	}

	xmmsc_result_unref (res);

	return ret;
}

void
cmd_volume (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;
	int i;
	GList *channels, *cur;
	gchar *end = NULL;
	gint vol;
	gboolean vol_rel = FALSE;

	if (argc < 3) {
		print_error ("You must specify a volume level.");
	}

	vol = strtol (argv[argc - 1], &end, 10);
	if (end == argv[argc - 1]) {
		print_error ("Please specify a number from 0-100.");
	}

	if (*argv[argc - 1] == '+' || *argv[argc - 1] == '-') {
		vol_rel = TRUE;
	}

	channels = NULL;
	for (i = 2; i < argc - 1; i++) {
		volume_channel_t *chan = g_new (volume_channel_t, 1);
		chan->name = argv[i];
		chan->volume = volume_get (conn, argv[i]);
		channels = g_list_prepend (channels, chan);
	}

	if (!channels) {
		res = xmmsc_playback_volume_get (conn);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);

		if (xmmsv_get_error (val, &errmsg)) {
			print_error ("Failed to get channel information: %s", errmsg);
		}

		xmmsv_dict_foreach (val, get_keys, &channels);

		xmmsc_result_unref (res);
	}

	for (cur = channels; cur; cur = g_list_next (cur)) {
		volume_channel_t *chan = cur->data;

		if (vol_rel) {
			chan->volume += vol;
		} else {
			chan->volume = vol;
		}

		res = xmmsc_playback_volume_set (conn, chan->name, chan->volume);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);

		if (xmmsv_get_error (val, &errmsg)) {
			print_error ("Failed to set volume: %s", errmsg);
		}

		xmmsc_result_unref (res);
	}

	g_list_free (channels);
}


void
cmd_volume_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	const char *errmsg;

	res = xmmsc_playback_volume_get (conn);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &errmsg)) {
		print_error ("Failed to get volume: %s", errmsg);
	}
	xmmsv_dict_foreach (val, print_hash, NULL);

	xmmsc_result_unref (res);
}
