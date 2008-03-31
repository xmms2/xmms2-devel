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
 * @file XMMS2 mDNS agent
 */

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/utsname.h>

#include <glib.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

static GMainLoop *ml;
static AvahiEntryGroup *group = NULL;
static guint32 port;
static gchar *name;

static void
disconnected (void *data)
{
	exit (0);
}

static void
handle_quit (xmmsc_result_t *res, void *data)
{
	g_main_loop_quit ((GMainLoop *) data);
}

static void
group_callback (AvahiEntryGroup *g,
                AvahiEntryGroupState state,
                void *userdata)
{
	g_return_if_fail (g == group);

	switch (state) {
		case AVAHI_ENTRY_GROUP_ESTABLISHED:
			printf ("_xmms2._tcp port: %d is registered\n", port);
			break;
		case AVAHI_ENTRY_GROUP_COLLISION:
			printf ("Got group collsion... terminating...\n");
			g_main_loop_quit (ml);
			break;
		case AVAHI_ENTRY_GROUP_FAILURE:
			g_main_loop_quit (ml);
			break;
		case AVAHI_ENTRY_GROUP_UNCOMMITED:
		case AVAHI_ENTRY_GROUP_REGISTERING:
			;
	}

}

static void
create_services (AvahiClient *c)
{
	int ret;
	struct utsname uts;
	gchar *name;

	g_return_if_fail (c);

	if (uname (&uts) != 0) {
		printf ("failed to run uname()\n");
		name = "xmms2";
	} else {
		name = g_strdup (uts.nodename);
	}

	if (!group) {
		if (!(group = avahi_entry_group_new (c, group_callback, NULL))) {
			printf ("couldn't create new group!\n");
			g_main_loop_quit (ml);
		}
	}

	if ((ret = avahi_entry_group_add_service (group, AVAHI_IF_UNSPEC,
	                                          AVAHI_PROTO_UNSPEC,
	                                          0, name, "_xmms2._tcp",
	                                          NULL, NULL, port, NULL)) < 0)
	{
		printf ("couldn't add entry to group: %s\n", avahi_strerror (ret));
		g_free (name);
		g_main_loop_quit (ml);
	}

	g_free (name);

	if ((ret = avahi_entry_group_commit (group)) < 0) {
		printf ("couldn't commit group: %s\n", avahi_strerror (ret));
		g_main_loop_quit (ml);
	}

	return;
}

static void
client_callback (AvahiClient *c,
                 AvahiClientState state,
                 void *userdata)
{
	g_return_if_fail (c);

	switch (state) {
		case AVAHI_CLIENT_S_RUNNING:
			if (!group) {
				create_services (c);
			}
			break;
		case AVAHI_CLIENT_S_COLLISION:
			printf ("Client collision, terminating...");
			g_main_loop_quit (ml);
			break;
		case AVAHI_CLIENT_FAILURE:
			printf ("Client failure: %s\n", avahi_strerror (avahi_client_errno (c)));
			g_main_loop_quit (ml);
			break;
		case AVAHI_CLIENT_CONNECTING:
		case AVAHI_CLIENT_S_REGISTERING:
			;
	}
}

void
register_service (void)
{
	int error;
	const AvahiPoll *poll_api;
	AvahiGLibPoll *glib_poll;
	AvahiClient *client = NULL;

	name = avahi_strdup ("XMMS2 mDNS Agent");

	glib_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
	poll_api = avahi_glib_poll_get (glib_poll);

	client = avahi_client_new (poll_api, 0, client_callback, NULL, &error);
	if (!client) {
		printf ("Failed to create Avahi client: %s\n", avahi_strerror (error));
		avahi_client_free (client);
		avahi_glib_poll_free (glib_poll);
	}

}

int
main (int argc, char **argv)
{
	xmmsc_connection_t *conn;
	gchar *path;
	gchar *gp = NULL;
	gchar **s;
	gchar **ipcsplit;
	int i;

	printf ("Starting XMMS2 mDNS Agent...\n");

	path = getenv ("XMMS_PATH_FULL");
	if (!path) {
		printf ("Sorry you need XMMS_PATH_FULL set\n");
		exit (1);
	}

	ipcsplit = g_strsplit (path, ";", 0);
	for (i = 0; ipcsplit[i]; i++) {
		if (g_ascii_strncasecmp (ipcsplit[i], "tcp://", 6) == 0) {
			gp = ipcsplit[i];
		}
	}

	if (!gp) {
		printf ("Need to have a socket listening to TCP before we can do that!\n");
		exit (1);
	}

	s = g_strsplit (gp, ":", 0);
	if (s && s[2]) {
		port = strtol (s[2], NULL, 10);
	} else {
		port = XMMS_DEFAULT_TCP_PORT;
	}

	conn = xmmsc_init ("xmms2-mdns");

	if (!conn) {
		printf ("Could not init xmmsc_connection!\n");
		exit (1);
	}

	if (!xmmsc_connect (conn, gp)) {
		printf ("Could not connect to xmms2d: %s\n", xmmsc_get_last_error (conn));
		exit (1);
	}

	avahi_set_allocator (avahi_glib_allocator ());

	ml = g_main_loop_new (NULL, FALSE);

	XMMS_CALLBACK_SET (conn, xmmsc_broadcast_quit, handle_quit, ml);
	xmmsc_disconnect_callback_set (conn, disconnected, NULL);
	
	register_service ();
	
	xmmsc_mainloop_gmain_init (conn);

	g_main_loop_run (ml);

	xmmsc_unref (conn);
	printf ("XMMS2-mDNS shutting down...\n");

	g_main_loop_unref (ml);

	avahi_free (name);

	return 0;
}

