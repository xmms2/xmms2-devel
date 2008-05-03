/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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

	res = xmmsc_main_stats (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_dict_foreach (res, print_hash, NULL);
	xmmsc_result_unref (res);
}


void
cmd_plugin_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;
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

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		const gchar *shortname, *desc;

		if (xmmsc_result_get_dict_entry_string (res, "shortname", &shortname) &&
		    xmmsc_result_get_dict_entry_string (res, "description", &desc)) {
			print_info ("%s - %s", shortname, desc);
		}

		xmmsc_result_list_next (res);
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

	if (argc < 3) {
		print_error ("Need to specify a URL to browse");
	}

	res = xmmsc_xform_media_browse (conn, argv[2]);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	for (;xmmsc_result_list_valid (res); xmmsc_result_list_next (res)) {
		xmmsc_result_value_type_t type;
		const gchar *r;
		gint d;

		type = xmmsc_result_get_dict_entry_type (res, "realpath");
		if (type != XMMSC_RESULT_VALUE_TYPE_NONE) {
			xmmsc_result_get_dict_entry_string (res, "realpath", &r);
		} else {
			xmmsc_result_get_dict_entry_string (res, "path", &r);
		}

		xmmsc_result_get_dict_entry_int (res, "isdir", &d);
		print_info ("%s%c", r, d ? '/' : ' ');
	}

	xmmsc_result_unref (res);
}
