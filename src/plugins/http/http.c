#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Type definitions
 */

typedef struct {
	gint fd;
} xmms_http_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_http_can_handle (const gchar *uri);
static gboolean xmms_http_open (xmms_transport_t *transport, const gchar *uri);
static gint xmms_http_read (xmms_transport_t *transport, gchar *buffer, guint len);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "http",
			"HTTP transport " VERSION,
			"HTTP streaming transport");
	
	xmms_plugin_method_add (plugin, "can_handle", xmms_http_can_handle);
	xmms_plugin_method_add (plugin, "open", xmms_http_open);
	xmms_plugin_method_add (plugin, "read", xmms_http_read);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_http_can_handle (const gchar *uri)
{
	g_return_val_if_fail (uri, FALSE);

	XMMS_DBG ("xmms_http_can_handle (%s)", uri);
	
	if ((g_strncasecmp (uri, "http:", 5) == 0))
		return TRUE;

	return FALSE;
}

gint
x_connect (const gchar *server, const gchar *path, gint port)
{

	struct sockaddr_in sa;
	struct hostent *hp;
	gint sock;

	if ((hp = gethostbyname (server)) == NULL) {
		XMMS_DBG ("Host not found!");
		return 0;
	}

	memcpy (&sa.sin_addr, hp->h_addr, hp->h_length);
	sa.sin_family = hp->h_addrtype;
	sa.sin_port = g_htons (port);

	if ((sock = socket (hp->h_addrtype, SOCK_STREAM, 0)) == -1) {
		XMMS_DBG ("socket error!");
		free (hp);
		return 0;
	}

	if (connect (sock, (struct sockaddr *)&sa, sizeof (sa)) == -1) {
		XMMS_DBG ("connect error!");
		free (hp);
		return 0;
	}

	return sock;

}

static gboolean
xmms_http_open (xmms_transport_t *transport, const gchar *uri)
{
	xmms_http_data_t *data;
	const gchar *server;
	const gchar *path;
	gchar request[1024];
	gchar *p;
	gint port;
	gint fd;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (uri, FALSE);

	/* http://server:port/path */

	server = strchr (uri, ':');
	server=server+3;/* skip // */
	
	if ( (p = strchr (server, ':')) ) {
		*p='\0';
		*p++;

		if (!(path = strchr (p, '/'))) {
			XMMS_DBG ("Malformated URI");
			return FALSE;
		}
		
		port = strtol (p, NULL, 10);
	} else {
		path = strchr (server, '/');
		*path='\0';
		*path++;
		port = 80;
		if (!path) {
			XMMS_DBG ("Malformated URI");
			return FALSE;
		}
	}

	XMMS_DBG ("Host: %s, port: %d, path: %s", server, port, path);


	fd = x_connect (server, path, port);

	if (!fd)
		return FALSE;

	XMMS_DBG ("connected!");

	g_snprintf (request, 1024, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: XMMS/" VERSION "\r\n\r\n", path, server);

	XMMS_DBG ("%s", request);

	send (fd, request, strlen (request), 0);

	data = g_new0 (xmms_http_data_t, 1);

	data->fd = fd;

	xmms_transport_plugin_data_set (transport, data);

	xmms_transport_mime_type_set (transport, "audio/mpeg");
	
	return TRUE;
}

static gint
xmms_http_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	xmms_http_data_t *data;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	data = xmms_transport_plugin_data_get (transport);
	g_return_val_if_fail (data, -1);

	ret = recv (data->fd, buffer, len, 0);

	return ret;
}

