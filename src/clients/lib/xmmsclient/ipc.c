#include <glib.h>

#include "xmms/signal_xmms.h"
#include "xmms/ipc_transport.h"
#include "xmms/ipc_msg.h"

#include "xmms/util.h"
#include "xmms/ringbuf.h"
#include "xmms/ipc.h"

typedef void (*xmmsc_ipc_wakeup_t) (xmmsc_ipc_t *);

struct xmmsc_ipc_St {
	GMutex *mutex;
	xmms_ringbuf_t *write_buffer;
	xmms_ringbuf_t *read_buffer;
	xmms_ipc_transport_t *transport;
	gchar *error;
	gpointer private_data;
	gboolean disconnect;
	xmmsc_ipc_wakeup_t wakeup;
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
	ipc->disconnect = FALSE;
	ipc->pollopts = XMMSC_IO_IN;

	return ipc;
}

void
xmmsc_ipc_wakeup_set (xmmsc_ipc_t *ipc, xmmsc_ipc_wakeup_t wakeupfunc)
{
	ipc->wakeup = wakeupfunc;
}

void
xmmsc_ipc_exec_msg (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg)
{
	printf ("Executing msg cmd id %d\n", msg->cmd);
}

gboolean
xmmsc_ipc_io_in_callback (xmmsc_ipc_t *ipc)
{
	xmms_ipc_msg_t *msg;
	gint ret;
	g_return_val_if_fail (ipc, FALSE);

	do {
		ret = xmms_ipc_transport_read (ipc->transport, buffer, 4096);
		if (ret == -1) {
			break;
		}
		g_mutex_lock (ipc->mutex);
		xmms_ringbuf_write (ipc->read_buffer, buffer, ret);
		g_mutex_unlock (ipc->mutex);
	} while (ret > 0);

	printf ("got %d bytes in ringbuffer!\n", xmms_ringbuf_bytes_used (ipc->read_buffer));

	g_mutex_lock (ipc->mutex);
	do {
		if (!xmms_ipc_msg_can_read (ipc->read_buffer)) {
			break;
		} else {
			msg = xmms_ipc_msg_read (ipc->read_buffer);
			g_mutex_unlock (ipc->mutex);
			printf ("Read msg with command %d\n", msg->cmd);
			xmmsc_ipc_exec_msg (msg);
			g_mutex_lock (ipc->mutex);
		}
	} while (msg);
	g_mutex_unlock (ipc->mutex);

	return TRUE;
}

gboolean
xmmsc_ipc_io_out_callback (xmmsc_ipc_t *ipc)
{
	g_return_val_if_fail (ipc, FALSE);
	gint len;

	g_mutex_lock (ipc->mutex);
	while (xmms_ringbuf_bytes_used (ipc->write_buffer)) {
		gchar buf[4096];
		gint ret, iret;

		len = MIN (xmms_ringbuf_bytes_used (ipc->write_buffer), 4096);
		ret = xmms_ringbuf_read (ipc->write_buffer, buf, len);
		iret = xmms_ipc_transport_write (ipc->transport, buf, ret);
		if (iret == -1)
			break;
	}
	g_mutex_unlock (ipc->mutex);

	return TRUE;

}

gboolean
xmmsc_ipc_msg_write (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg)
{
	g_return_val_if_fail (ipc, FALSE);
	g_return_val_if_fail (msg, FALSE);

	g_mutex_lock (ipc->mutex);
	if (!xmms_ipc_msg_write (ipc->write_buffer, msg)) {
		g_mutex_unlock (ipc->mutex);
		return FALSE;
	}
	g_mutex_unlock (ipc->mutex);

	/* we need to tell the binding that we want to poll on OUT to. */
	ipc->pollopts = XMMSC_IO_OUT | XMMSC_IO_IN;
	ipc->wakeup (ipc);
}

void
xmmsc_ipc_disconnect_set (xmmsc_ipc_t *ipc)
{
	ipc->disconnect = TRUE;
	/* Maybe we should have a callback here ? */
}

gpointer
xmmsc_ipc_private_data_get (xmmsc_ipc_t *ipc)
{
	g_return_val_if_fail (ipc, NULL);
	return ipc->private_data;
}

void
xmmsc_ipc_private_data_set (xmmsc_ipc_t *ipc, gpointer data)
{
	g_return_if_fail (ipc);
	ipc->private_data = data;
}

void
xmmsc_ipc_destroy (xmmsc_ipc_t *ipc)
{
	xmms_ringbuf_destroy (ipc->write_buffer);
	xmms_ringbuf_destroy (ipc->read_buffer);
	g_mutex_free (ipc->mutex);
	if (ipc->transport) {
		xmms_ipc_transport_destroy (ipc->transport);
	}
	if (ipc->error) {
		g_free (ipc->error);
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

