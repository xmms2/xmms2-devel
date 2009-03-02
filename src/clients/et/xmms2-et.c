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

/**
 * @file XMMS2 Phone home test coverage reporting client.
 */

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/utsname.h>

#include <glib.h>

#define DEST_IP "78.47.20.162"
#define DEST_PORT 24944

static time_t start_time;
static const gchar *server_version = "unknown";
static gchar *output_plugin;
static const gchar *system_name = "unknown";
static gint mlib_resolves = 0;

static int send_socket;
static struct sockaddr_in dest_addr;

static void
send_msg (const gchar *status, GString *data)
{
	GString *str;
	time_t now;

	str = g_string_new ("");

	now = time (NULL);

	g_string_append_printf (str, "XMMS2-ET %s\n", status);
	g_string_append_printf (str, "version=%s\n", server_version);
	g_string_append_printf (str, "system=%s\n", system_name);
	g_string_append_printf (str, "output=%s\n", output_plugin);
	g_string_append_printf (str, "starttime=%ld\n", start_time);
	g_string_append_printf (str, "uptime=%ld\n", now - start_time);
	g_string_append_printf (str, "mlibresolves=%d\n", mlib_resolves);

	mlib_resolves = 0;

	if (data)
		g_string_append (str, data->str);

	sendto (send_socket, str->str, str->len, 0, (struct sockaddr *)&dest_addr, sizeof (dest_addr));

	g_string_free (str, TRUE);
}

static void
disconnected (void *data)
{
	send_msg ("Unclean shutdown", NULL);
	exit (0);
}

static int
handle_quit (xmmsv_t *v, void *data)
{
	g_main_loop_quit ((GMainLoop *) data);

	return FALSE; /* stop the broadcast */
}

static int
handle_mediainfo_reader (xmmsv_t *v, void *userdata)
{
	static gint last_unindexed;
	gint unindexed;

	if (xmmsv_get_int (v, &unindexed)) {
		if (unindexed < last_unindexed) {
			mlib_resolves += last_unindexed - unindexed;
		}
		last_unindexed = unindexed;
	}

	return TRUE; /* restart the signal */
}

static int
handle_mediainfo (xmmsv_t *v, void *userdata)
{
	static const gchar *props[] = {"chain", NULL};
	static const gchar *pref[] = {"server", NULL};
	GString *str;
	const gchar *tstr;
	gint tint, i;
	xmmsv_t *dict;

	str = g_string_new ("");

	/* convert propdict to dict */
	dict = xmmsv_propdict_to_dict (v, pref);

	for (i = 0; props[i]; i++) {
		switch (xmmsv_dict_entry_get_type (dict, props[i])) {
		case XMMSV_TYPE_STRING:
			if (xmmsv_dict_entry_get_string (dict, props[i], &tstr)) {
				g_string_append_printf (str, "%s=%s\n", props[i], tstr);
			}
			break;
		case XMMSV_TYPE_INT32:
			if (xmmsv_dict_entry_get_int (dict, props[i], &tint)) {
				g_string_append_printf (str, "%s=%d\n", props[i], tint);
			}
			break;
		default:
			/* noop */
			break;
		}
	}

	send_msg ("New media", str);

	g_string_free (str, TRUE);
	return TRUE; /* keep broadcast alive */
}

static int
handle_current_id (xmmsv_t *v, void *userdata)
{
	xmmsc_connection_t *conn = userdata;
	xmmsc_result_t *res2;
	gint current_id;

	if (xmmsv_get_int (v, &current_id)) {
		res2 = xmmsc_medialib_get_info (conn, current_id);
		xmmsc_result_notifier_set (res2, handle_mediainfo, conn);
		xmmsc_result_unref (res2);
	}

	return TRUE; /* keep broadcast alive */
}

static int
handle_config (xmmsv_t *v, void *userdata)
{
	const gchar *value;

	if (!xmmsv_dict_entry_get_string (v, "output.plugin", &value))
		return TRUE;

	g_free (output_plugin);
	output_plugin = g_strdup (value);

	return TRUE;
}

static int
handle_config_val (xmmsv_t *v, void *userdata)
{
	const gchar *value;

	if (!xmmsv_get_string (v, &value))
		return TRUE;

	g_free (output_plugin);
	output_plugin = g_strdup (value);

	return TRUE;
}

static int
handle_stats (xmmsv_t *v, void *userdata)
{
	const gchar *tstr;

	if (xmmsv_dict_entry_get_string (v, "version", &tstr)) {
		server_version = g_strdup (tstr);
	}

	return TRUE;
}

static void
get_systemname (void)
{
	struct utsname uts;

	if (uname (&uts) >= 0) {
		system_name = g_strdup_printf ("%s %s %s",
		                               uts.sysname,
		                               uts.release,
		                               uts.machine);
	}
}


int
main (int argc, char **argv)
{
	xmmsc_connection_t *conn;
	GMainLoop *ml;
	gchar *path;

	printf ("Starting XMMS2 phone home agent...\n");

	path = getenv ("XMMS_PATH");

	conn = xmmsc_init ("xmms2-et");

	if (!conn) {
		printf ("Could not init xmmsc_connection!\n");
		exit (1);
	}

	if (!xmmsc_connect (conn, path)) {
		printf ("Could not connect to xmms2d: %s\n", xmmsc_get_last_error (conn));
		exit (1);
	}

	output_plugin = g_strdup ("unknown");

	get_systemname ();

	send_socket = socket (PF_INET, SOCK_DGRAM, 0);

	ml = g_main_loop_new (NULL, FALSE);

	memset (&dest_addr, 0, sizeof (dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons (DEST_PORT);
	inet_aton (DEST_IP, &dest_addr.sin_addr);

	start_time = time (NULL);

	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_playback_current_id, handle_current_id, conn);
	XMMS_CALLBACK_SET (conn, xmmsc_main_stats, handle_stats, NULL);
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_configval_changed, handle_config, NULL);
	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_quit, handle_quit, ml);

	XMMS_CALLBACK_SET (conn, xmmsc_signal_mediainfo_reader_unindexed, handle_mediainfo_reader, NULL);

	{
		xmmsc_result_t *res;
		res = xmmsc_configval_get (conn, "output.plugin");
		xmmsc_result_notifier_set (res, handle_config_val, NULL);
		xmmsc_result_unref (res);
	}

	xmmsc_disconnect_callback_set (conn, disconnected, NULL);

	xmmsc_mainloop_gmain_init (conn);

	g_main_loop_run (ml);

	xmmsc_unref (conn);

	printf ("XMMS2-ET shutting down...\n");
	send_msg ("Clean shutdown", NULL);

	return 0;
}
