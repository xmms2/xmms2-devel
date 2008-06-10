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

static void
handle_quit (xmmsc_result_t *res, void *data)
{
	g_main_loop_quit ((GMainLoop *) data);
}

static void
handle_mediainfo_reader (xmmsc_result_t *res, void *userdata)
{
	static guint last_unindexed;
	xmmsc_result_t *newres;
	guint unindexed;

	if (!xmmsc_result_get_uint (res, &unindexed))
		return;

	if (unindexed < last_unindexed) {
		mlib_resolves += last_unindexed - unindexed;
	}
	last_unindexed = unindexed;

	newres = xmmsc_result_restart (res);
	xmmsc_result_unref (res);
	xmmsc_result_unref (newres);
}

static void
handle_mediainfo (xmmsc_result_t *res, void *userdata)
{
	static const gchar *props[] = {"chain", NULL};
	static const gchar *pref[] = {"server", NULL};
	GString *str;
	const gchar *tstr;
	gint tint, i;
	guint tuint;

	str = g_string_new ("");

	xmmsc_result_source_preference_set (res, pref);

	for (i = 0; props[i]; i++) {
		switch (xmmsc_result_get_dict_entry_type (res, props[i])) {
		case XMMSC_RESULT_VALUE_TYPE_STRING:
			if (xmmsc_result_get_dict_entry_string (res, props[i], &tstr)) {
				g_string_append_printf (str, "%s=%s\n", props[i], tstr);
			}
			break;
		case XMMSC_RESULT_VALUE_TYPE_UINT32:
			if (xmmsc_result_get_dict_entry_uint (res, props[i], &tuint)) {
				g_string_append_printf (str, "%s=%u\n", props[i], tuint);
			}
			break;
		case XMMSC_RESULT_VALUE_TYPE_INT32:
			if (xmmsc_result_get_dict_entry_int (res, props[i], &tint)) {
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
}

static void
handle_current_id (xmmsc_result_t *res, void *userdata)
{
	xmmsc_connection_t *conn = userdata;
	xmmsc_result_t *res2;
	guint current_id;

	if (!xmmsc_result_get_uint (res, &current_id))
		return;

	res2 = xmmsc_medialib_get_info (conn, current_id);
	xmmsc_result_notifier_set (res2, handle_mediainfo, conn);
	xmmsc_result_unref (res2);
}

static void
handle_config (xmmsc_result_t *res, void *userdata)
{
	const gchar *value;

	if (!xmmsc_result_get_dict_entry_string (res, "output.plugin", &value))
		return;

	g_free (output_plugin);
	output_plugin = g_strdup (value);
}

static void
handle_config_val (xmmsc_result_t *res, void *userdata)
{
	const gchar *value;

	if (!xmmsc_result_get_string (res, &value))
		return;

	g_free (output_plugin);
	output_plugin = g_strdup (value);
}

static void
handle_stats (xmmsc_result_t *res, void *userdata)
{
	const gchar *tstr;

	if (xmmsc_result_get_dict_entry_string (res, "version", &tstr)) {
		server_version = g_strdup (tstr);
	}
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
