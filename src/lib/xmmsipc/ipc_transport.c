#include <glib.h>

#include "ipc_transport.h"


gint
xmms_ipc_transport_read (xmms_ipc_transport_t *ipct, gchar *buffer, gint len)
{
	return ipct->read_func (ipct, buffer, len);
}

gint
xmms_ipc_transport_write (xmms_ipc_transport_t *ipct, gchar *buffer, gint len)
{
	return ipct->write_func (ipct, buffer, len);
}

gint
xmms_ipc_transport_fd_get (xmms_ipc_transport_t *ipct)
{
	g_return_val_if_fail (ipct, -1);
	return ipct->fd;
}

xmms_ipc_transport_t *
xmms_ipc_server_accept (xmms_ipc_transport_t *ipct)
{
	g_return_val_if_fail (ipct, NULL);

	if (!ipct->accept_func)
		return NULL;

	return ipct->accept_func (ipct);
}

xmms_ipc_transport_t *
xmms_ipc_client_init (gchar *path)
{
	xmms_ipc_transport_t *transport = NULL;

	g_return_val_if_fail (path, NULL);

	if (g_strncasecmp (path, "unix://", 7) == 0) {
		transport = xmms_ipc_usocket_client_init (path+7);
	} else if (g_strncasecmp (path, "tcp://", 6) == 0) {
/*		transport = xmms_ipc_tcp_server_init (path+6);*/
	}

	return transport;
}

xmms_ipc_transport_t *
xmms_ipc_server_init (gchar *path)
{
	xmms_ipc_transport_t *transport = NULL;

	g_return_val_if_fail (path, NULL);

	if (g_strncasecmp (path, "unix://", 7) == 0) {
		transport = xmms_ipc_usocket_server_init (path+7);
	} else if (g_strncasecmp (path, "tcp://", 6) == 0) {
/*		transport = xmms_ipc_tcp_server_init (path+6);*/
	}

	return transport;
}
