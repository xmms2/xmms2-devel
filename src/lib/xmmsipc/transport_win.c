#include <stdlib.h>
#include <xmmsc/xmmsc_ipc_transport.h>
#include "socket_tcp.h"
#include <xmmsc/xmmsc_stringport.h>
#include <xmmsc/xmmsc_util.h>

xmms_ipc_transport_t *
xmms_ipc_client_init (const char *path)
{
	xmms_ipc_transport_t *transport = NULL;
	xmms_url_t *url;

	x_return_val_if_fail (path, NULL);

	url = parse_url (path);
	x_return_val_if_fail (url, NULL);

	if (!strcasecmp (url->protocol, "") || !strcasecmp (url->protocol, "unix")) {
		transport = NULL;
	} else if (!strcasecmp (url->protocol, "tcp")) {
		transport = xmms_ipc_tcp_client_init (url, url->ipv6_host);
	}

	free_url (url);
	return transport;
}

xmms_ipc_transport_t *
xmms_ipc_server_init (const char *path)
{
	xmms_ipc_transport_t *transport = NULL;
	xmms_url_t *url;

	x_return_val_if_fail (path, NULL);

	url = parse_url (path);
	x_return_val_if_fail (url, NULL);

	if (!strcasecmp (url->protocol, "") || !strcasecmp (url->protocol, "unix")) {
		transport = NULL;
	} else if (!strcasecmp (url->protocol, "tcp")) {
		transport = xmms_ipc_tcp_server_init (url, url->ipv6_host);
	}

	free_url (url);
	return transport;
}
