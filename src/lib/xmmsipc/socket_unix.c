
#include <glib.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>

#include "xmms/util.h"
#include "xmms/ipc_transport.h"

void
xmms_ipc_usocket_destroy (xmms_ipc_transport_t *ipct)
{
	XMMS_DBG ("Closing socket %d", ipct->fd);
	g_free (ipct->path);
	close (ipct->fd);
	g_free (ipct);
}

gint
xmms_ipc_usocket_read (xmms_ipc_transport_t *ipct, gchar *buffer, gint len)
{
	gint fd;
	gint ret;
	g_return_val_if_fail (ipct, -1);
	g_return_val_if_fail (buffer, -1);

	fd = ipct->fd;

	ret =  recv (fd, buffer, len, 0);
	XMMS_DBG ("ret == %d", ret);
	if (ret == -1) {
		XMMS_DBG ("error %s (%d)", strerror (errno), errno);
	}

	return ret;
}

gint
xmms_ipc_usocket_write (xmms_ipc_transport_t *ipct, gchar *buffer, gint len)
{
	gint fd;
	g_return_val_if_fail (ipct, -1);
	g_return_val_if_fail (buffer, -1);
	
	fd = ipct->fd;

	return send (fd, buffer, len, 0);

}

xmms_ipc_transport_t *
xmms_ipc_usocket_client_init (gchar *path)
{
	gint fd;
	gint flags;
	xmms_ipc_transport_t *ipct;
	struct sockaddr_un saddr;


	fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		return NULL;
	}

	saddr.sun_family = AF_UNIX;
	strncpy (saddr.sun_path, path, 108);

	if (connect (fd, (struct sockaddr *) &saddr, sizeof (saddr)) == -1) {
		close (fd);
		return NULL;
	}

	flags = fcntl (fd, F_GETFL, 0);

	if (flags == -1) {
		close (fd);
		return NULL;
	}

	flags |= O_NONBLOCK;

	flags = fcntl (fd, F_SETFL, flags);
	if (flags == -1) {
		close (fd);
		return NULL;
	}
		
	ipct = g_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = g_strdup (path);
	ipct->read_func = xmms_ipc_usocket_read;
	ipct->write_func = xmms_ipc_usocket_write;
	ipct->destroy_func = xmms_ipc_usocket_destroy;

	return ipct;
}

xmms_ipc_transport_t *
xmms_ipc_usocket_accept (xmms_ipc_transport_t *transport)
{
	gint fd;
	struct sockaddr_un sin;
	socklen_t sin_len;

	g_return_val_if_fail (transport, NULL);

	fd = accept (transport->fd, (struct sockaddr *)&sin, &sin_len);
	if (fd >= 0) {
		gint flags;
		xmms_ipc_transport_t *ret;

		flags = fcntl (fd, F_GETFL, 0);

		if (flags == -1) {
			close (fd);
			return NULL;
		}

		flags |= O_NONBLOCK;

		flags = fcntl (fd, F_SETFL, flags);
		if (flags == -1) {
			close (fd);
			return NULL;
		}


		ret = g_new0 (xmms_ipc_transport_t, 1);
		ret->fd = fd;
		ret->read_func = xmms_ipc_usocket_read;
		ret->write_func = xmms_ipc_usocket_write;
		ret->destroy_func = xmms_ipc_usocket_destroy;

		return ret;
	} else {
		XMMS_DBG ("Accept error %s (%d)", strerror (errno), errno);
	}

	return NULL;
}

xmms_ipc_transport_t *
xmms_ipc_usocket_server_init (gchar *path)
{
	gint fd;
	gint flags;
	xmms_ipc_transport_t *ipct;
	struct sockaddr_un saddr;


	fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		return NULL;
	}

	saddr.sun_family = AF_UNIX;
	strncpy (saddr.sun_path, path, 108);

	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		unlink (path);
	}

	if (bind (fd, (struct sockaddr *) &saddr, sizeof (saddr)) == -1) {
		close (fd);
		return NULL;
	}

	listen (fd, 5);

	flags = fcntl (fd, F_GETFL, 0);

	if (flags == -1) {
		close (fd);
		return NULL;
	}

	flags |= O_NONBLOCK;

	flags = fcntl (fd, F_SETFL, flags);
	if (flags == -1) {
		close (fd);
		return NULL;
	}
		
	ipct = g_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = g_strdup (path);
	ipct->read_func = xmms_ipc_usocket_read;
	ipct->write_func = xmms_ipc_usocket_write;
	ipct->accept_func = xmms_ipc_usocket_accept;
	ipct->destroy_func = xmms_ipc_usocket_destroy;

	return ipct;
}

