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
#include <errno.h>

/*
 * Type definitions
 */

typedef struct {
	gint fd;
	gint state;
	gchar *path;
	gchar *server;
	gchar *request;
	struct sockaddr_in *sa;
	struct hostent *hp;
} xmms_http_data_t;

extern int errno;

/*
 * Function prototypes
 */

static gboolean xmms_http_can_handle (const gchar *uri);
static gboolean xmms_http_init (xmms_transport_t *transport, const gchar *uri);
static gint xmms_http_read (xmms_transport_t *transport, gchar *buffer, guint len);
static gboolean x_request (xmms_transport_t *transport);

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

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_http_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, xmms_http_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_http_read);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SEEK);
	
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

static gboolean
xmms_http_init (xmms_transport_t *transport, const gchar *uri)
{
	xmms_http_data_t *data;
	gchar *server;
	gchar *path;
	gchar *p;
	gint port;
	gint fd;
	struct sockaddr_in *sa;
	struct hostent *hp;


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
		if (!*path) {
			XMMS_DBG ("Malformated URI");
			return FALSE;
		}
	}

	XMMS_DBG ("Host: %s, port: %d, path: %s", server, port, path);

	if ((hp = gethostbyname (server)) == NULL) {
		XMMS_DBG ("Host not found!");
		return -1;
	}

	sa = calloc(1,sizeof(struct sockaddr_in));
	
	memcpy (&(sa->sin_addr), hp->h_addr, hp->h_length);
	sa->sin_family = hp->h_addrtype;
	sa->sin_port = g_htons (port);

	if ((fd = socket (hp->h_addrtype, SOCK_STREAM, 0)) == -1) {
		XMMS_DBG ("socket error!");
		free (hp);
		return -1;
	}
	free (hp);

	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		XMMS_DBG ("Error when changing from blocking to nonblocking socket\n");
		return -1;
	}
	
	if (connect (fd, (struct sockaddr *)sa, sizeof (struct sockaddr_in)) == -1 && errno != EAGAIN) {
		XMMS_DBG ("connect error!");
		return -1;
	}

	data = g_new0 (xmms_http_data_t, 1);

	data->fd = fd;
	data->server = server;
	data->path = path;
	data->sa = sa;

	xmms_transport_plugin_data_set (transport, data);

	return x_request(transport);
}
static gboolean
x_request (xmms_transport_t *transport)
{
	xmms_http_data_t *data;
	gint sent;
	gint reqlen;
	
	data = xmms_transport_plugin_data_get (transport);
	if (data->state == 0){
		if (connect (data->fd, (struct sockaddr *)data->sa, sizeof (struct sockaddr_in)) == -1 && 
				errno != EAGAIN && 
				errno != EISCONN) {
			XMMS_DBG ("connect error!");
			return -1;
		} else if (errno == EAGAIN) { 
			return TRUE;
		} else {

			data->state ++;
			XMMS_DBG ("connected!");
			data->request = calloc(1,1024);
			g_snprintf (data->request, 1024, "GET /%s HTTP/1.1\r\n"
				   "Host: %s\r\n"
				   "Connection: close\r\n"
				   "User-Agent: XMMS/" VERSION "\r\n\r\n", 
				   data->path, data->server);

			XMMS_DBG ("%s", data->request);
		}
	}

	reqlen = strlen(data->request);
	
	while (reqlen > 0) {
		sent = send (data->fd, data->request, reqlen, 0);
		if(sent == -1 && errno == EAGAIN){
			xmms_transport_plugin_data_set (transport, data);
			return TRUE;
		} else if(sent == -1){
			return -1;
		} else {
			data->request += sent;
			reqlen -= sent;
		}
	}

	data->state ++;
	xmms_transport_plugin_data_set (transport, data);

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

	switch(data->state){
		case 0: 
		case 1:
			x_request(transport); return 0;
		default: 	
		/** @todo FIXME: !*/
		xmms_transport_mime_type_set (transport, "audio/mpeg");
		
		ret = recv (data->fd, buffer, len, 0);
	
		return ret;
	}
}

