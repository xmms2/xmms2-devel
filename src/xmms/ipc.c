#include <glib.h>
#include "ipc_transport.h"

#include "xmms/util.h"
#include "xmms/ringbuf.h"

struct xmms_ipc_client_t {
	xmms_transport_t *transport;
	gint status;
};

struct xmms_ipc_t {
	xmms_ipc_transport_t *transport;
	GList *clients;
	xmms_ringbuf_t *write_buffer;
	gint signal_array[XMMS_IPC_SIGNAL_END];
};

xmms_ipc_client_t *
xmms_ipc_client_new (xmms_ipc_transport_t *transport)
{
	g_return_val_if_fail (transport, NULL);

	client = g_new0 (xmms_ipc_client_t, 1);
	client->status = XMMS_IPC_CLIENT_STATUS_NEW;
	client->transport = transport;

	return client;
}

gboolean
xmms_ipc_source_prepare (GSource *source, gint *timeout_)
{
	/* No timeout here */
	return FALSE;
}

gboolean
xmms_ipc_source_check (GSource *source)
{
	/* Maybe check for errors here? */
	return TRUE;
}

gboolean
xmms_ipc_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	callback (source, user_data);
	return TRUE;
}

gboolean
xmms_ipc_source_accept (GSource *source, xmms_ipc_t *ipc)
{
	xmms_ipc_transport_t *transport;
	xmms_ipc_client_t *client;

	XMMS_DBG ("Client connect?!");
	transport = xmms_ipc_transport_accept (ipc->transport);
	if (!transport)
		return FALSE;

	client = xmms_ipc_client_new (transport);

	ipc->clients = g_list_append (ipc->clients, client);

	return TRUE;
}

gboolean
xmms_ipc_setup_with_gmain (xmms_ipc_t *ipc)
{
	GPollFD pollfd;
	GSource *source;
	GSourceFuncs sfuncs;

	pollfd.fd = xmms_ipc_transport_fd_get (ipc->transport);
	pollfd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;

	sfuncs.dispatch = xmms_ipc_source_dispatch;
	sfuncs.check = xmms_ipc_source_check;
	sfuncs.prepare = xmms_ipc_source_prepare;
	
	source = g_source_new (&sfuncs, sizeof (GSource));

	g_source_set_callback (source, xmms_ipc_source_accept, (gpointer) ipc, NULL);

}

xmms_ipc_t *
xmms_ipc_init (gchar *path)
{
	xmms_ipc_transport_t *transport; 
	xmms_ipc_t *ipc;
	
	g_return_val_if_fail (main, NULL);
	g_return_val_if_fail (path, NULL);
	
	transport = xmms_ipc_server_init (path);
	if (!transport) {
		return NULL;
	}

	ipc = g_new0 (xmms_ipc_t, 1);
	ipc->transport = transport;
	ipc->write_buffer = xmms_ringbuf_new (XMMS_MSG_DATA_SIZE * 10);

	return ipc;
}

