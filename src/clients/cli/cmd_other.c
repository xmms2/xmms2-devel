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
#include "cmd_other.h"
#include "common.h"

void
cmd_stats (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;

	res = xmmsc_main_stats (conn);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	xmmsv_dict_foreach (val, print_hash, NULL);
	xmmsc_result_unref (res);
}


void
cmd_plugin_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	xmmsv_list_iter_t *it;
	xmms_plugin_type_t type = XMMS_PLUGIN_TYPE_ALL;

	if (argc > 2) {
		if (g_ascii_strcasecmp (argv[2], "output") == 0) {
			type = XMMS_PLUGIN_TYPE_OUTPUT;
		} else if (g_ascii_strcasecmp (argv[2], "xform") == 0) {
			type = XMMS_PLUGIN_TYPE_XFORM;
		} else {
			print_error ("no such plugin type!");
		}
	}

	res = xmmsc_plugin_list (conn, type);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	xmmsv_get_list_iter (val, &it);
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_t *dict;
		const gchar *shortname, *desc;

		if (xmmsv_list_iter_entry (it, &dict) &&
		    xmmsv_dict_entry_get_string (dict, "shortname", &shortname) &&
		    xmmsv_dict_entry_get_string (dict, "description", &desc)) {
			print_info ("%s - %s", shortname, desc);
		}

		xmmsv_list_iter_next (it);
	}
	xmmsc_result_unref (res);
}


void
cmd_quit (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_quit (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
}

void
cmd_browse (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
	xmmsv_list_iter_t *it;
	xmmsv_t *val;

	if (argc < 3) {
		print_error ("Need to specify a URL to browse");
	}

	res = xmmsc_xform_media_browse (conn, argv[2]);
	xmmsc_result_wait (res);
	val = xmmsc_result_get_value (res);

	if (xmmsv_is_error (val)) {
		print_error ("%s", xmmsv_get_error_old (val));
	}

	xmmsv_get_list_iter (val, &it);
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_t *dict;
		xmmsv_type_t type;
		const gchar *r;
		gint d;

		xmmsv_list_iter_entry (it, &dict);
		type = xmmsv_dict_entry_get_type (dict, "realpath");
		if (type != XMMSV_TYPE_NONE) {
			xmmsv_dict_entry_get_string (dict, "realpath", &r);
		} else {
			xmmsv_dict_entry_get_string (dict, "path", &r);
		}

		xmmsv_dict_entry_get_int (dict, "isdir", &d);
		print_info ("%s%c", r, d ? '/' : ' ');
		xmmsv_list_iter_next (it);
	}

	xmmsc_result_unref (res);
}
