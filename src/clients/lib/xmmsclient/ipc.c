#include <glib.h>

#include "xmms/signal_xmms.h"
#include "xmms/ipc_transport.h"
#include "xmms/ipc_msg.h"

#include "xmms/util.h"
#include "xmms/ringbuf.h"
#include "xmms/ipc.h"

struct xmmsc_ipc_St {
	GMutex *mutex;
	xmms_ringbuf_t *write_buffer;
	xmms_ringbuf_t *read_buffer;
	xmms_ipc_transport_t *transport;
	gchar *error;
};

typedef struct xmmsc_ipc_St xmms_ipc_t;

xmmsc_ipc_t *
xmmsc_ipc_init (void)
{
	xmmsc_ipc_t *ipc;
	ipc = g_new0 (xmmsc_ipc_t, 1);
	ipc->mutex = g_mutex_new ();
	ipc->write_buffer = xmms_ringbuf_new (XMMS_IPC_MSG_SIZE * 10);
	ipc->read_buffer = xmms_ringbuf_new (XMMS_IPC_MSG_SIZE * 10);

	return ipc;
}

void
xmmsc_ipc_free (xmmsc_ipc_t *ipc)
{
	xmms_ringbuf_destroy (ipc->write_buffer);
	xmms_ringbuf_destroy (ipc->read_buffer);
	g_mutex_free (ipc->mutex);
	if (ipc->transport) {
		xmms_ipc_transport_destroy (ipc->transport);
	}
	g_free (ipc);
}

gboolean
xmmsc_ipc_connect (xmmsc_ipc_t *ipc, gchar *path) 
{
	g_return_val_if_fail (path, FALSE);

	ipc->transport = xmms_ipc_client_init (path);
	if (tranport) {
		ipc->error = "Could not init client!";
		return FALSE;
	}

	return TRUE;
}

