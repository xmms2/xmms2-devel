#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/xmms.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <unistd.h>
#include <netinet/in.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/*
 * Type definitions
 */
typedef enum { NONE, RESOLVED, CONNECTED, REQUEST_SENT } state_t;

typedef struct {
	gint fd;
	state_t state;
	gchar *path;
	gchar *server;
	gchar *request;
	gchar *requestp;
	struct sockaddr *sa;
	gint salen;
	gint ctimeout;
	struct addrinfo *res;
} xmms_http_data_t;

extern int errno;

/*
 * Function prototypes
 */

static gboolean xmms_http_can_handle (const gchar *uri);
static gboolean xmms_http_init (xmms_transport_t *transport, const gchar *uri);
static gint xmms_http_read (xmms_transport_t *transport, gchar *buffer, guint len);
static gboolean x_request (xmms_transport_t *transport);
static gint xmms_http_size (xmms_transport_t *transport);
static void xmms_http_close (xmms_transport_t *transport);
static gboolean x_trynextaddr(xmms_transport_t *transport);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "http",
			"HTTP transport " XMMS_VERSION,
			"HTTP streaming transport");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_http_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_http_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_http_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_http_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_http_size);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SEEK);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_http_can_handle (const gchar *uri)
{
	gchar *dec;
	g_return_val_if_fail (uri, FALSE);
	
	dec = xmms_util_decode_path (uri);

	XMMS_DBG ("xmms_http_can_handle (%s)", dec);
	
	if ((g_strncasecmp (dec, "http:", 5) == 0)) {
		g_free (dec);
		return TRUE;
	}

	g_free (dec);

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
	gint error;
	struct addrinfo hints;
	struct addrinfo *res;
	gchar *nuri;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (uri, FALSE);

	/* http://server:port/path */

	nuri = xmms_util_decode_path (uri);

	server = strchr (nuri, ':');
	server = server + 3; /* skip // */
	
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
		if(!path) {
			XMMS_DBG ("Malformated URI");
			return FALSE;
		}
		*path='\0';
		port = 80;
	}

	XMMS_DBG ("Host: %s, port: %d, path: %s", server, port, path);


	memset(&hints, 0, sizeof(hints));
	/* set-up hints structure */
	hints.ai_family = PF_UNSPEC;
	error = getaddrinfo(server, NULL, &hints, &res);
	if (error) {
		XMMS_DBG ( "Couldn't resolve");
		/* XMMS_DBG (gai_strerror(error) ); */
		freeaddrinfo(res);
		return FALSE;
	}
		
	data = g_new0 (xmms_http_data_t, 1);

	data->server = server;
	data->path = path;
	xmms_transport_plugin_data_set (transport, data);

	if (!res) {
		return FALSE;
	}

	data->state = RESOLVED;
	data->res = res;
	xmms_transport_mime_type_set (transport, "audio/mpeg");

	return x_trynextaddr(transport);
}
static gboolean
x_trynextaddr(xmms_transport_t *transport){
	struct sockaddr *sa;
	gint salen;
	gchar namebuf[1024];
	xmms_http_data_t *data;
	gint fd;

	data = xmms_transport_plugin_data_get (transport);

	/** Get next address from list */
	if (!data->res) {
		data->sa = NULL;
		return FALSE;
	}
	sa = (struct sockaddr *)data->res->ai_addr;
	salen = data->res->ai_addrlen;

	data->ctimeout = 0;
	data->sa = sa;
	data->salen = salen;
	data->res = data->res->ai_next;

	if(!getnameinfo(sa, salen, namebuf, sizeof(namebuf), NULL, 0, NI_NUMERICHOST)) {
		XMMS_DBG ("Trying %s...", namebuf);
	}
	
	if ((fd = socket ( ((struct sockaddr_storage *)sa)->ss_family, SOCK_STREAM, 0)) == -1) {
		XMMS_DBG ( "%s", strerror(errno));
		return FALSE;
	}
	data->fd = fd;

	if (fcntl (fd, F_SETFL, O_NONBLOCK) < 0) {
		XMMS_DBG ("Error when changing from blocking to nonblocking socket\n");
		return FALSE;
	}
		
	if (connect (fd, sa, salen) == -1 && (errno != EINTR && errno != EAGAIN && errno != EINPROGRESS) ) {
		XMMS_DBG ("connect error: %s", strerror(errno) );
		return FALSE;
	}

	return x_request(transport) || (data->res ? TRUE : FALSE); /** Come back here for more? */
}
static gboolean
x_request (xmms_transport_t *transport)
{
	xmms_http_data_t *data;
	gint sent;
	gint reqlen;

	data = xmms_transport_plugin_data_get (transport);
	if (data->state == RESOLVED){
		fd_set fdlist;
		struct timeval tv;
		int ret;

		FD_ZERO (&fdlist);
		FD_SET (data->fd, &fdlist);

		tv.tv_sec = 20;
		tv.tv_usec = 0;

		XMMS_DBG ( "Polling %d...", data->fd);
		ret = select (data->fd+1, NULL, &fdlist, NULL, &tv);
		if (ret == 0) {
			XMMS_DBG ("select returned 0");
			data->ctimeout ++;
			if (data->ctimeout > 60) { /* Don't try this again, try next addr instead */
				return x_trynextaddr (transport);
			} else {
				return TRUE;
			}
		} else if (ret == -1) {
			XMMS_DBG ("Select returned -1");
			return x_trynextaddr (transport); /* Error condition, try next addr */
		} else {
			XMMS_DBG ("select returned %d", ret);
			data->state = CONNECTED;
		}

		freeaddrinfo(data->res);
		data->res = NULL;
		XMMS_DBG ("connected!");
		data->request = g_malloc(1024);
		if(data->path[0] == '/'){
			g_snprintf (data->request, 1024, "GET %s HTTP/1.1\r\n"
				   "Host: %s\r\n"
				   "Connection: close\r\n"
				   "User-Agent: XMMS/" XMMS_VERSION "\r\n\r\n", 
				   data->path, data->server);
		} else {
			g_snprintf (data->request, 1024, "GET /%s HTTP/1.1\r\n"
				   "Host: %s\r\n"
				   "Connection: close\r\n"
				   "User-Agent: XMMS/" XMMS_VERSION "\r\n\r\n", 
				   data->path, data->server);
		}
		data->requestp = data->request;
		XMMS_DBG ("%s", data->request);
	}

	reqlen = strlen(data->requestp);
	
	while (reqlen > 0) {
		sent = send (data->fd, data->requestp, reqlen, 0);
		if (sent == -1 && (errno == EAGAIN || errno == EINTR) ){
			xmms_transport_plugin_data_set (transport, data);
			return TRUE;
		} else if (sent == -1){
			return FALSE;
		} else {
			data->requestp += sent;
			reqlen -= sent;
		}
	}

	data->state = REQUEST_SENT;
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

	XMMS_DBG ("state: %d", data->state);

	switch(data->state){
		case NONE:
		case RESOLVED:
/*			x_trynextaddr(transport);*/
		case CONNECTED:
			if(!x_request(transport)){
				return -1;
			} else {
				return 0;
			}
		case REQUEST_SENT:
		default: 	
		/** @todo FIXME: !*/
		
		ret = recv (data->fd, buffer, len-1, 0);
		XMMS_DBG ("buffer (%d): %s", len,buffer);
	
		return ret;
	}
}

static void
xmms_http_close (xmms_transport_t *transport)
{
	xmms_http_data_t *data;
	g_return_if_fail (transport);

	data = xmms_transport_plugin_data_get (transport);
	g_return_if_fail (data);
	
	if (data->state == CONNECTED){
		close (data->fd);
		g_free (data->request);
		g_free (data->sa);
	}
	if (data->state == RESOLVED){
		g_free (data->sa);
	}
	g_free (data);
}

static gint
xmms_http_size (xmms_transport_t *transport)
{
	return 0;
}
