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
	guint type;

	if (argc < 3) {
		type = XMMS_PLUGIN_TYPE_ALL;
	} else if (g_strcasecmp (argv[2], "output") == 0) {
		type = XMMS_PLUGIN_TYPE_OUTPUT;
	} else if (g_strcasecmp (argv[2], "transport") == 0) {
		type = XMMS_PLUGIN_TYPE_TRANSPORT;
	} else if (g_strcasecmp (argv[2], "decoder") == 0) {
		type = XMMS_PLUGIN_TYPE_DECODER;
	} else if (g_strcasecmp (argv[2], "effect") == 0) {
		type = XMMS_PLUGIN_TYPE_EFFECT;
	} else if (g_strcasecmp (argv[2], "playlist") == 0) {
		type = XMMS_PLUGIN_TYPE_PLAYLIST;
	} else {
		print_error ("no such plugin type!");
	}

	res = xmmsc_plugin_list (conn, type);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		gchar *shortname, *desc;

		if (xmmsc_result_get_dict_entry_str (res, "shortname", &shortname) &&
		    xmmsc_result_get_dict_entry_str (res, "description", &desc)) {
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

