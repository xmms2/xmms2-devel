#include <glib.h>
#include <dns_sd.h>

#include <xmms/xmms_log.h>

#include "daap_mdns_browse.h"

typedef struct daap_mdns_dnssd_St {
	DNSServiceRef browse_client;
	GPollFD *browse_fd;
	GPollFD *resolve_fd;
	GPollFD *query_fd;
	GSource *source;
} daap_mdns_dnssd_t;

static GSList *server_list;
static GMutex *mutex;

void
daap_mdns_dnssd_free (daap_mdns_dnssd_t *sd)
{
	g_return_if_fail (sd);
	if (sd->pollfd) {
		g_source_remove_poll (sd->source, sd->pollfd);
		g_free (sd->pollfd);
	}
	if (sd->source) {
		g_source_destroy (sd->source);
	}
	DNSServiceRefDeallocate (sd->browse_client);
}

static gboolean
mdns_source_prepare (GSource *source, gint *timeout_)
{
	/* No timeout here */
	return FALSE;
}

static gboolean
mdns_source_check (GSource *source)
{
	/* Maybe check for errors here? */
	return TRUE;
}

static gboolean
mdns_source_dispatch (GSource *source,
                      GSourceFunc callback,
                      gpointer user_data)
{
	daap_mdns_dnssd_t *sd = user_data;
	DNSServiceErrorType err;

	if ((sd->pollfd->revents & G_IO_ERR) || (sd->pollfd->revents & G_IO_HUP)) {
		return FALSE;
	} else if (sd->pollfd->revents & G_IO_IN) {
		XMMS_DBG ("Processing events!");
		err = DNSServiceProcessResult (sd->client);
		if (err != kDNSServiceErr_NoError) {
			xmms_log_error ("DNSServiceProcessResult returned error");
			return FALSE;
		}
	}

	return TRUE;
}

static void
qr_reply (DNSServiceRef sdRef,
		  DNSServiceFlags flags,
		  uint32_t ifIndex,
		  DNSServiceErrorType errorCode,
		  const char *fullname,
		  uint16_t rrtype,
		  uint16_t rrclass,
		  uint16_t rdlen,
		  const void *rdata,
		  uint32_t ttl,
		  void *context)
{
	daap_mdns_server_t *server = context;
	gchar addr[1000];
	const gchar *rd = (gchar *)rdata;

	g_return_if_fail (server);
	g_return_if_fail (rrtype == kDNSServiceType_A);

	g_snprintf (addr, 1000, "%d.%d.%d.%d", rd[0], rd[1], rd[2], rd[3]);

	server->address = g_strdup (addr);

	XMMS_DBG ("adding server %s %s", server->mdns_hostname, server->address);
	g_mutex_lock (mutex);
	server_list = g_slist_prepend (server_list, server);
	g_mutex_unlock (mutex);

}


static void
resolve_reply (DNSServiceRef client,
			   DNSServiceFlags flags,
			   uint32_t ifIndex,
			   DNSServiceErrorType errorCode,
			   const char *fullname,
			   const char *hosttarget,
			   uint16_t opaqueport,
			   uint16_t txtLen,
			   const char *txtRecord,
			   void *context)
{
	daap_mdns_server_t *server = context;
	DNSServiceErrorType err;
	union { guint16 s; guchar b[2]; } portu = { opaqueport };

	g_return_if_fail (server);

	server->port = ((guint16) portu.b[0]) << 8 | portu.b[1];
	server->mdns_hostname = g_strdup (hosttarget);

	XMMS_DBG ("Resolved %s", server->mdns_hostname);

	err = DNSServiceQueryRecord (&client, 0,
								 kDNSServiceInterfaceIndexAny,
								 server->mdns_hostname,
								 kDNSServiceType_A,
								 kDNSServiceClass_IN,
								 qr_reply, server);

	if (err != kDNSServiceErr_NoError) {
		xmms_log_error ("Error from QueryRecord!");
		g_free (server->mdns_hostname);
		g_free (server->server_name);
		g_free (server);
		return;
	}

	DNSServiceRefDeallocate (client);
}

static void
browse_reply (DNSServiceRef client,
			  DNSServiceFlags flags,
			  uint32_t ifIndex,
			  DNSServiceErrorType errorCode,
			  const char *replyName,
			  const char *replyType,
			  const char *replyDomain,
			  void *context)
{
	DNSServiceErrorType err;
	DNSServiceRef client2;
	daap_mdns_server_t *server;
	daap_mdns_dnssd_t *sd = context;
	gboolean remove = (flags & kDNSServiceFlagsAdd) ? FALSE : TRUE;

	if (!remove) {
		server = g_new0 (daap_mdns_server_t, 1);
		server->server_name = g_strdup (replyName);
		/*
		server->address = g_strdup (ad);
		server->mdns_hostname = g_strdup (hostname);
		server->port = port;
		*/
		XMMS_DBG ("Looking up server %s", server->server_name);
		err = DNSServiceResolve (&client2, 0, kDNSServiceInterfaceIndexAny,
								 server->server_name,
								 "_daap._tcp", "local",
								 resolve_reply, server);
		if (err != kDNSServiceErr_NoError) {
			xmms_log_error ("Couldn't do ServiceResolv");
			g_free (server->server_name);
			g_free (server);
			return;
		}

		if (!add_poll (sd, client2, sd->resolve_fd)) {
			xmms_log_error ("Couldn't setup poll!");
		}
	}

}

static GSourceFuncs mdns_poll_funcs = {
	mdns_source_prepare,
	mdns_source_check,
	mdns_source_dispatch,
	NULL
};

static gboolean
add_poll (daap_mdns_dnssd_t *dnssd, DNSServiceRef client, GPollFD *fd)
{
	int fd;
	DNSServiceErrorType err;

	fd = DNSServiceRefSockFD (&client);
	if (fd == -1) {
		return FALSE
	}

	fd>fd = fd;
	fd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	g_source_add_poll (dnssd->source, dnssd->pollfd);

	return TRUE;
}

gboolean
daap_mdns_initialize()
{
	daap_mdns_dnssd_t *dnssd;
	DNSServiceErrorType err;
	DNSServiceRef client;

	dnssd = g_new0 (daap_mdns_dnssd_t, 1);

	err = DNSServiceBrowse (&client, 0, kDNSServiceInterfaceIndexAny,
							"_daap._tcp", 0, browse_reply, NULL);

	if (err != kDNSServiceErr_NoError) {
		g_free (dnssd);
		xmms_log_error ("Couldn't setup mDNS poller");
		return FALSE;
	}
	
	mutex = g_mutex_new ();
	dnssd->source = g_source_new (&mdns_poll_funcs, sizeof (GSource));
	g_source_set_callback (dnssd->source, (GSourceFunc) mdns_source_dispatch,
						   dnssd, NULL);
	dnssd->browse_fd = g_new0 (GPollFD, 1);
	dnssd->request_fd = g_new0 (GPollFD, 1);
	dnssd->query_fd = g_new0 (GPollFD, 1);

	if (!add_poll (client, dnssd->browse_fd)) {
		goto fail;
	}
	
	g_source_attach (dnssd->source, NULL);

	xmms_log_info ("Initialized mDNS client");

	return TRUE;

fail:
	daap_mdns_dnssd_free (dnssd);
	return FALSE;
}

GSList *
daap_mdns_get_server_list ()
{
	GSList * l;
	g_mutex_lock (mutex);
	l = g_slist_copy (server_list);
	g_mutex_unlock (mutex);
	return l;
}

