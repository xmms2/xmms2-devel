/** @file daap_mdns_browse.c
 *  Browser for DAAP servers shared via mDNS.
 *
 *  Copyright (C) 2006 XMMS2 Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "daap_mdns_browse.h"

#include <glib.h>

#if MDNS_USING_AVAHI
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>
#endif

#define ADDR_LEN (3 * 4 + 3 + 1) /* standard dotted-quad fmt */

#if MDNS_USING_AVAHI
typedef struct {
	AvahiClient *client;
	GMainLoop *mainloop;
} browse_callback_userdata_t;

static GSList *g_server_list = NULL;
static AvahiGLibPoll *gl_poll = NULL;
static AvahiClient *client;
static AvahiServiceBrowser *browser;

static void
daap_mdns_resolve_cb (AvahiServiceResolver *resolv,
                      AvahiIfIndex iface,
                      AvahiProtocol proto,
                      AvahiResolverEvent event,
                      const gchar *name,
                      const gchar *type,
                      const gchar *domain,
                      const gchar *hostname,
                      const AvahiAddress *addr,
                      guint16 port,
                      AvahiStringList *text,
                      AvahiLookupResultFlags flags,
                      void *userdata)
{
	gboolean *remove = userdata;
	gchar ad[ADDR_LEN];
	daap_mdns_server_t *server;

	if (!resolv) {
		return;
	}

	switch (event) {
		case AVAHI_RESOLVER_FOUND:
			server = (daap_mdns_server_t *)
			         g_malloc0 (sizeof (daap_mdns_server_t));
			avahi_address_snprint (ad, sizeof (ad), addr);

			server->server_name = g_strdup (name);
			server->address = g_strdup (ad);
			server->mdns_hostname = g_strdup (hostname);
			server->port = port;

			if (*remove) {
				/* FIXME if a server is removed after avahi is initialized,
				 * Bad Things happen on browse. This is because the entry
				 * in the list isn't _actually_ removed... */
				g_server_list = g_slist_remove (g_server_list, server);
			} else {
				g_server_list = g_slist_prepend (g_server_list, server);
			}
			g_free (remove);

			break;

		case AVAHI_RESOLVER_FAILURE:
			break;
		
		default:
			break;
	}

	avahi_service_resolver_free (resolv);
}

static void
daap_mdns_browse_cb (AvahiServiceBrowser *browser,
                     AvahiIfIndex iface,
                     AvahiProtocol proto,
                     AvahiBrowserEvent event,
                     const gchar *name,
                     const gchar *type,
                     const gchar *domain,
                     AvahiLookupResultFlags flags,
                     void *userdata)
{
	gboolean ok = FALSE;
	gboolean *b = g_malloc (sizeof (gboolean));

	AvahiClient *client = ((browse_callback_userdata_t *) userdata)->client;

	if (!browser) {
		return;
	}

	switch (event) {
		case AVAHI_BROWSER_NEW:
			*b = FALSE;
			ok = (gboolean)
			     avahi_service_resolver_new (client, iface, proto, name, type,
			                                 domain, AVAHI_PROTO_UNSPEC, 0,
			                                 daap_mdns_resolve_cb, b);
			break;

		case AVAHI_BROWSER_REMOVE:
			*b = TRUE;
			ok = (gboolean)
			     avahi_service_resolver_new (client, iface, proto, name, type,
			                                 domain, AVAHI_PROTO_UNSPEC, 0,
			                                 daap_mdns_resolve_cb, b);
			break;

		case AVAHI_BROWSER_CACHE_EXHAUSTED:
			break;

		case AVAHI_BROWSER_ALL_FOR_NOW:
			break;

		default:
			break;
	}
}

static void
daap_mdns_client_cb (AvahiClient *client,
                     AvahiClientState state,
                     void * userdata)
{
	if (!client) {
		return;
	}

	switch (state) {
		case AVAHI_CLIENT_FAILURE:
			break;
		default:
			break;
	}
}

static void
daap_mdns_timeout (AvahiTimeout *to, void *userdata)
{
}

static gboolean
daap_mdns_timeout_glib (void *userdata)
{
	return FALSE;
}
#endif

gboolean
daap_mdns_initialize ()
{
#if MDNS_USING_AVAHI
	const AvahiPoll *av_poll;

	GMainLoop *ml = NULL;

	gboolean ok = TRUE;
	gint errval;
	struct timeval tv;
	browse_callback_userdata_t *browse_userdata;

	if (gl_poll) {
		ok = FALSE;
		goto fail;
	}

	browse_userdata = g_malloc0 (sizeof (browse_callback_userdata_t));

	avahi_set_allocator (avahi_glib_allocator ());

	ml = g_main_loop_new (NULL, FALSE);

	gl_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
	av_poll = avahi_glib_poll_get (gl_poll);
	
	avahi_elapse_time (&tv, 2000, 0);
	av_poll->timeout_new (av_poll, &tv, daap_mdns_timeout, NULL);
	g_timeout_add (5000, daap_mdns_timeout_glib, ml);

	client = avahi_client_new (av_poll, 0, daap_mdns_client_cb, ml, &errval);
	if (!client) {
		ok = FALSE;
		goto fail;
	}

	browse_userdata->client = client;
	browse_userdata->mainloop = ml;

	browser = avahi_service_browser_new (client, AVAHI_IF_UNSPEC,
	                                     AVAHI_PROTO_UNSPEC, "_daap._tcp", NULL,
	                                     0, daap_mdns_browse_cb,
	                                     browse_userdata);
	if (!browser) {
		ok = FALSE;
		goto fail;
	}

fail:
	return ok;
#else
	return FALSE;
#endif
}

GSList *
daap_mdns_get_server_list ()
{
#if MDNS_USING_AVAHI
	GSList * l;
	l = g_slist_copy (g_server_list);
	return l;
#else
	return NULL;
#endif
}

