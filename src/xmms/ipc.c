#include <glib.h>

#include "xmms/signal_xmms.h"
#include "xmms/ipc_transport.h"
#include "xmms/ipc_msg.h"

#include "xmms/util.h"
#include "xmms/ringbuf.h"
#include "xmms/ipc.h"

struct xmms_ipc_St {
	xmms_ipc_transport_t *transport;
	GList *clients;
	gint signal_array[XMMS_IPC_SIGNAL_END];
	GSource *source;
	GPollFD *pollfd;
	GCond *msg_cond;
	GMutex *mutex;
	gboolean run;
};

typedef struct xmms_ipc_client_St {
	GMutex *mutex;
	GPollFD *gpoll;
	GSource *gsource;
	GQueue *msgs;
	xmms_ipc_transport_t *transport;
	xmms_ringbuf_t *write_buffer;
	xmms_ringbuf_t *read_buffer;
	gint status;
	gint poll_opts;
	xmms_ipc_t *ipc;
} xmms_ipc_client_t;

typedef gboolean (*xmms_ipc_server_callback_t) (GSource *, xmms_ipc_t *);
typedef gboolean (*xmms_ipc_client_callback_t) (GSource *, xmms_ipc_client_t *);

xmms_ipc_client_t *
xmms_ipc_client_new (xmms_ipc_t *ipc, xmms_ipc_transport_t *transport)
{
	xmms_ipc_client_t *client;

	g_return_val_if_fail (transport, NULL);

	client = g_new0 (xmms_ipc_client_t, 1);
	client->status = XMMS_IPC_CLIENT_STATUS_NEW;
	client->transport = transport;
	client->write_buffer = xmms_ringbuf_new (XMMS_IPC_MSG_DATA_SIZE * 10);
	client->read_buffer = xmms_ringbuf_new (XMMS_IPC_MSG_DATA_SIZE * 10);
	client->poll_opts = G_IO_IN | G_IO_HUP | G_IO_ERR;
	client->ipc = ipc;
	client->mutex = g_mutex_new ();
	client->msgs = g_queue_new ();

	return client;
}

void
xmms_ipc_client_destroy (xmms_ipc_client_t *client)
{
	xmms_ipc_msg_t *msg;

	if (client->gsource && client->gpoll) {
		g_source_remove_poll (client->gsource, client->gpoll);
		g_free (client->gpoll);
	}
	if (client->gsource) {
		g_source_remove (g_source_get_id (client->gsource));
		g_source_destroy (client->gsource);
	}

	g_mutex_lock (client->ipc->mutex);
	client->ipc->clients = g_list_remove (client->ipc->clients, client);
	g_mutex_unlock (client->ipc->mutex);

	while ((msg = g_queue_pop_head (client->msgs))) {
		xmms_ipc_msg_destroy (msg);
	}

	g_queue_free (client->msgs);

	xmms_ipc_transport_destroy (client->transport);
	xmms_ringbuf_destroy (client->write_buffer);
	xmms_ringbuf_destroy (client->read_buffer);
	g_mutex_free (client->mutex);
	g_free (client);
}

gboolean
xmms_ipc_client_msg_write (xmms_ipc_client_t *client, xmms_ipc_msg_t *msg)
{
	g_return_val_if_fail (client, FALSE);
	g_return_val_if_fail (msg, FALSE);

	g_mutex_lock (client->mutex);
	if (!xmms_ipc_msg_write (client->write_buffer, msg)) {
		g_mutex_unlock (client->mutex);
		return FALSE;
	}
	client->poll_opts = G_IO_IN | G_IO_OUT | G_IO_HUP | G_IO_ERR;
	xmms_ipc_client_setup_with_gmain (client);
	g_mutex_unlock (client->mutex);

	XMMS_DBG ("Making noice!");
	g_main_context_wakeup (NULL);

	return TRUE;
}

xmms_ipc_msg_t *
xmms_ipc_client_msg_read (xmms_ipc_client_t *client)
{
	xmms_ipc_msg_t *msg;
	g_return_val_if_fail (client, NULL);

	g_mutex_lock (client->mutex);
	msg = xmms_ipc_msg_read (client->read_buffer);
	g_mutex_unlock (client->mutex);

	return msg;
}
	
gboolean
xmms_ipc_client_source_prepare (GSource *source, gint *timeout_)
{
	/* No timeout here */
	return FALSE;
}

gboolean
xmms_ipc_client_source_check (GSource *source)
{
	return TRUE;
}

gboolean
xmms_ipc_client_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	((xmms_ipc_server_callback_t)callback) (source, user_data);
	return TRUE;
}

gboolean
xmms_ipc_client_source_callback (GSource *source, xmms_ipc_client_t *client)
{
	xmms_ipc_transport_t *transport;
	xmms_ipc_msg_t *msg;

	XMMS_DBG ("Client event?!");

	if (client->gpoll->revents & G_IO_HUP) {
		XMMS_DBG ("client hangup!");
		xmms_ipc_client_destroy (client);

		return FALSE;
	} else if (client->gpoll->revents & G_IO_IN) {
		gchar buffer[4096];
		gint ret=0;

		XMMS_DBG ("Client is spakking!");

		do {
			ret = xmms_ipc_transport_read (client->transport, buffer, 4096);
			if (ret == -1) {
				break;
			}
			g_mutex_lock (client->mutex);
			xmms_ringbuf_write (client->read_buffer, buffer, ret);
			g_mutex_unlock (client->mutex);
		} while (ret > 0);

		XMMS_DBG ("got %d bytes in ringbuffer!", xmms_ringbuf_bytes_used (client->read_buffer));
		g_mutex_lock (client->mutex);
		do {
			if (!xmms_ipc_msg_can_read (client->read_buffer)) {
				break;
			} else {
				msg = xmms_ipc_msg_read (client->read_buffer);
				g_queue_push_tail (client->msgs, msg);
				XMMS_DBG ("Read msg with command %d", msg->cmd);
			}
		} while (msg);
		client->poll_opts = G_IO_IN | G_IO_HUP | G_IO_ERR;
		xmms_ipc_client_setup_with_gmain (client);
		g_cond_signal (client->ipc->msg_cond);
		g_mutex_unlock (client->mutex);
			
	} else if (client->gpoll->revents & G_IO_OUT) {
		gint len;
		XMMS_DBG ("Writing to client");
		while (xmms_ringbuf_bytes_used (client->write_buffer)) {
			gchar buf[4096];
			gint ret;

			len = MIN (xmms_ringbuf_bytes_used (client->write_buffer), 4096);
			g_mutex_lock (client->mutex);
			ret = xmms_ringbuf_read (client->write_buffer, buf, len);
			g_mutex_unlock (client->mutex);
			xmms_ipc_transport_write (client->transport, buf, ret);
		}
	}

	return TRUE;
}

static GSourceFuncs xmms_ipc_client_funcs = {
	xmms_ipc_client_source_prepare,
	xmms_ipc_client_source_check,
	xmms_ipc_client_source_dispatch,
	NULL
};

gboolean
xmms_ipc_client_setup_with_gmain (xmms_ipc_client_t *client)
{
	GPollFD *pollfd;
	GSource *source;

	pollfd = g_new0 (GPollFD, 1);
	pollfd->fd = xmms_ipc_transport_fd_get (client->transport);
	pollfd->events = client->poll_opts;
	
	if (client->gsource) {
		if (client->gpoll) {
			g_source_remove_poll (client->gsource, client->gpoll);
			g_free (client->gpoll);
		}
	} else {
		client->gsource = g_source_new (&xmms_ipc_client_funcs, sizeof (GSource));
		g_source_attach (client->gsource, NULL);
		g_source_set_callback (client->gsource, (GSourceFunc)xmms_ipc_client_source_callback, (gpointer) client, NULL);
	}

	XMMS_DBG ("adding pollfd with events %d", pollfd->events);

	g_source_add_poll (client->gsource, pollfd);
	client->gpoll = pollfd;


	return TRUE;
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
	((xmms_ipc_client_callback_t)callback) (source, user_data);
	return TRUE;
}

gboolean
xmms_ipc_source_accept (GSource *source, xmms_ipc_t *ipc)
{
	xmms_ipc_transport_t *transport;
	xmms_ipc_client_t *client;

	if (!(ipc->pollfd->revents & G_IO_IN)) {
		return FALSE;
	}

	XMMS_DBG ("Client connect?!");
	transport = xmms_ipc_server_accept (ipc->transport);
	if (!transport) {
		XMMS_DBG ("accept returned null!");
		return FALSE;
	}

	client = xmms_ipc_client_new (ipc, transport);

	if (xmms_ipc_client_setup_with_gmain (client)) {
		g_mutex_lock (ipc->mutex);
		ipc->clients = g_list_append (ipc->clients, client);
		g_mutex_unlock (ipc->mutex);
	}
	XMMS_DBG ("Client upset ;)");

	return TRUE;
}

static GSourceFuncs xmms_ipc_server_funcs = {
	xmms_ipc_source_prepare,
	xmms_ipc_source_check,
	xmms_ipc_source_dispatch,
	NULL
};

gboolean
xmms_ipc_setup_with_gmain (xmms_ipc_t *ipc)
{
	GSource *source;

	ipc->pollfd = g_new0 (GPollFD, 1);
	ipc->pollfd->fd = xmms_ipc_transport_fd_get (ipc->transport);
	ipc->pollfd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;

	source = g_source_new (&xmms_ipc_server_funcs, sizeof (GSource));
	ipc->source = source;

	g_source_set_callback (source, (GSourceFunc)xmms_ipc_source_accept, (gpointer) ipc, NULL);
	g_source_add_poll (source, ipc->pollfd);
	g_source_attach (source, NULL);

	return TRUE;
}

static gpointer
xmms_ipc_handle_thread (gpointer data)
{
	xmms_ipc_t *ipc = data;

	g_mutex_lock (ipc->mutex);
	while (ipc->run) {
		xmms_ipc_msg_t *msg;
		GList *c;

		for (c = ipc->clients; c; c = g_list_next (c)) {
			xmms_ipc_client_t *client = c->data;
			do {
				g_mutex_lock (client->mutex);
				msg = g_queue_pop_head (client->msgs);
				g_mutex_unlock (client->mutex);

				if (!msg) 
					break;

				XMMS_DBG ("Processing msg %d", msg->cmd);

				if (msg->cmd == 42) {
					gchar str[128];
					xmms_ipc_msg_get_string (msg, str, 128);
					XMMS_DBG ("String is %s", str);
				}

				xmms_ipc_msg_destroy (msg);
			} while (msg);
		}

		XMMS_DBG ("Waiting for msgs!");
		g_cond_wait (ipc->msg_cond, ipc->mutex);
		XMMS_DBG ("Got msgs...");
		
	}
	g_mutex_unlock (ipc->mutex);
}

xmms_ipc_t *
xmms_ipc_init (gchar *path)
{
	xmms_ipc_transport_t *transport; 
	xmms_ipc_t *ipc;
	
	g_return_val_if_fail (path, NULL);
	
	transport = xmms_ipc_server_init (path);
	if (!transport) {
		XMMS_DBG ("THE FAIL!");
		return NULL;
	}

	ipc = g_new0 (xmms_ipc_t, 1);
	ipc->transport = transport;
	ipc->mutex = g_mutex_new ();
	ipc->msg_cond = g_cond_new ();
	ipc->run = TRUE;

	g_thread_create (xmms_ipc_handle_thread, ipc, FALSE, NULL);

	XMMS_DBG ("IPC Initialized!");

	return ipc;
}

