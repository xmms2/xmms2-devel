#include <glib.h>
#include <string.h>

#include "xmms/signal_xmms.h"
#include "xmms/ipc_transport.h"
#include "xmms/ipc_msg.h"

#include "xmms/util.h"
#include "xmms/ringbuf.h"
#include "xmms/ipc.h"


struct xmms_ipc_St {
	xmms_ipc_transport_t *transport;
	GList *clients;
	GSource *source;
	GPollFD *pollfd;
	GCond *msg_cond;
	GMutex *mutex;
	gboolean run;

	xmms_object_t *objects[XMMS_IPC_OBJECT_END];

	xmms_object_t *signals[XMMS_IPC_SIGNAL_END];
	xmms_object_t *broadcasts[XMMS_IPC_SIGNAL_END];
};

struct xmms_ipc_client_St {
	GMutex *mutex;
	GPollFD *gpoll;
	GSource *gsource;
	GQueue *msgs;

	xmms_ipc_transport_t *transport;
	xmms_ringbuf_t *write_buffer;
	xmms_ringbuf_t *read_buffer;
	xmms_ipc_t *ipc;

	gint status;
	gint poll_opts;
	gint ref;

	guint pendingsignals[XMMS_IPC_SIGNAL_END];
	guint broadcasts[XMMS_IPC_SIGNAL_END];
};

static GMutex *global_ipc_lock;
static xmms_ipc_t *global_ipc;


static gboolean xmms_ipc_client_setup_with_gmain (xmms_ipc_client_t *client);


#define xmms_ipc_client_ref(a) do {\
	if (a) { \
		a->ref++;\
	} \
} while (0)

#define xmms_ipc_client_unref(a) do {\
	if (a) { \
		a->ref--;\
		if (a->ref == 0) {\
			xmms_ipc_client_destroy (a); \
			a = NULL; \
		} \
	} \
} while (0)

typedef gboolean (*xmms_ipc_client_callback_t) (GSource *, xmms_ipc_client_t *);
typedef gboolean (*xmms_ipc_server_callback_t) (GSource *, xmms_ipc_t *);

static xmms_ipc_client_t *
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
	
	xmms_ipc_client_ref (client);

	return client;
}

static void
xmms_ipc_client_destroy (xmms_ipc_client_t *client)
{
	xmms_ipc_msg_t *msg;

	XMMS_DBG ("Destroying client!");

	if (client->gsource && client->gpoll) {
		g_source_remove_poll (client->gsource, client->gpoll);
		g_free (client->gpoll);
		client->gpoll = NULL;
	}
	if (client->gsource) {
		g_source_remove (g_source_get_id (client->gsource));
		g_source_destroy (client->gsource);
		client->gsource = NULL;
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
	if (!xmms_ipc_msg_write (client->write_buffer, msg, msg->cid)) {
		g_mutex_unlock (client->mutex);
		return FALSE;
	}
	client->poll_opts = G_IO_IN | G_IO_OUT | G_IO_HUP | G_IO_ERR;
	xmms_ipc_client_setup_with_gmain (client);
	g_mutex_unlock (client->mutex);

	/* @todo: Should we take a ref to the client here? Yes maybe, but on the
	 * other hand, if the connections breaks the client can't get the data
	 * anyway...
	 */

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
	
static gboolean
xmms_ipc_client_source_prepare (GSource *source, gint *timeout_)
{
	/* No timeout here */
	return FALSE;
}

static gboolean
xmms_ipc_client_source_check (GSource *source)
{
	return TRUE;
}

static gboolean
xmms_ipc_client_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	((xmms_ipc_server_callback_t)callback) (source, user_data);
	return TRUE;
}

static gboolean
xmms_ipc_client_source_callback (GSource *source, xmms_ipc_client_t *client)
{
	xmms_ipc_msg_t *msg;
	gboolean disconnect = FALSE;

	g_mutex_lock (client->mutex);
	xmms_ipc_client_ref (client);
	g_mutex_unlock (client->mutex);

	if (client->gpoll->revents & G_IO_IN) {
		gchar buffer[4096];
		gint ret=0;

		do {
			ret = xmms_ipc_transport_read (client->transport, buffer, 4096);
			if (ret == -1) {
				break;
			} else if (ret == 0) {
				disconnect = TRUE;
				break;
			}
			g_mutex_lock (client->mutex);
			xmms_ringbuf_write (client->read_buffer, buffer, ret);
			g_mutex_unlock (client->mutex);
		} while (ret > 0);

		g_mutex_lock (client->mutex);
		do {
			if (!xmms_ipc_msg_can_read (client->read_buffer)) {
				break;
			} else {
				msg = xmms_ipc_msg_read (client->read_buffer);
				/* We ref this one time per msg we add */
				xmms_ipc_client_ref (client);
				g_queue_push_tail (client->msgs, msg);
			}
		} while (msg);
		g_cond_signal (client->ipc->msg_cond);
		g_mutex_unlock (client->mutex);
			
	} else if (client->gpoll->revents & G_IO_OUT) {
		gint len;
		g_mutex_lock (client->mutex);
		while (xmms_ringbuf_bytes_used (client->write_buffer)) {
			gchar buf[4096];
			gint ret;


			len = MIN (xmms_ringbuf_bytes_used (client->write_buffer), 4096);
			ret = xmms_ringbuf_read (client->write_buffer, buf, len);
			xmms_ipc_transport_write (client->transport, buf, ret);
		/*	xmms_ipc_client_unref (client); give back the ref that client_write took */
		}
		client->poll_opts = G_IO_IN | G_IO_HUP | G_IO_ERR;
		g_mutex_unlock (client->mutex);
	}

	xmms_ipc_client_unref (client);

	if (client && client->gpoll && client->gpoll->revents & G_IO_HUP) {
		disconnect = TRUE;
	}

	if (disconnect) {
		xmms_ipc_client_unref (client);

		return FALSE;
	} else if (client) {
		xmms_ipc_client_setup_with_gmain (client);
	}

	return TRUE;
}

static GSourceFuncs xmms_ipc_client_funcs = {
	xmms_ipc_client_source_prepare,
	xmms_ipc_client_source_check,
	xmms_ipc_client_source_dispatch,
	NULL
};

static gboolean
xmms_ipc_client_setup_with_gmain (xmms_ipc_client_t *client)
{
	GPollFD *pollfd;

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

	g_source_add_poll (client->gsource, pollfd);
	client->gpoll = pollfd;


	return TRUE;
}

static gboolean
xmms_ipc_source_prepare (GSource *source, gint *timeout_)
{
	/* No timeout here */
	return FALSE;
}

static gboolean
xmms_ipc_source_check (GSource *source)
{
	/* Maybe check for errors here? */
	return TRUE;
}

static gboolean
xmms_ipc_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	((xmms_ipc_client_callback_t)callback) (source, user_data);
	return TRUE;
}

static gboolean
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

static gboolean
type_and_msg_to_arg (xmms_object_cmd_arg_type_t type, xmms_ipc_msg_t *msg, xmms_object_cmd_arg_t *arg, gint i)
{
	switch (type) {
		case XMMS_OBJECT_CMD_ARG_UINT32 :
			if (!xmms_ipc_msg_get_uint32 (msg, &arg->values[i].uint32))
				return FALSE;
			arg->types[i] = type;
			break;
		case XMMS_OBJECT_CMD_ARG_INT32 :
			if (!xmms_ipc_msg_get_int32 (msg, &arg->values[i].int32))
				return FALSE;
			arg->types[i] = type;
			break;
		case XMMS_OBJECT_CMD_ARG_STRING :
			arg->values[i].string = g_new (gchar, 1024);
			if (!xmms_ipc_msg_get_string (msg, arg->values[i].string, 1024)) {
				g_free (arg->values[i].string);
				return FALSE;
			}
			arg->types[i] = type;
			break;
		default:
			XMMS_DBG ("Unknown value for a caller argument?");
			return FALSE;
			break;
	}
	return TRUE;
}

static void
hash_to_dict (gpointer key, gpointer value, gpointer udata)
{
        gchar *k = key;
        gchar *v = value;
	xmms_ipc_msg_t *msg = udata;

	if (v) {
		xmms_ipc_msg_put_string (msg, k);
		xmms_ipc_msg_put_string (msg, v);
	}
	
}

static void
xmms_ipc_handle_playlist_chmsg (xmms_ipc_msg_t *msg, xmms_playlist_changed_msg_t *chpl)
{
	xmms_ipc_msg_put_uint32 (msg, chpl->type);
	xmms_ipc_msg_put_uint32 (msg, chpl->id);
	xmms_ipc_msg_put_uint32 (msg, chpl->arg);
}

static void
xmms_ipc_do_hashtable (xmms_ipc_msg_t *msg, GHashTable *table)
{
	guint s = g_hash_table_size (table);
	xmms_ipc_msg_put_uint32 (msg, s);
	g_hash_table_foreach (table, hash_to_dict, msg);
}

static void
xmms_ipc_handle_arg_value (xmms_ipc_msg_t *msg, xmms_object_cmd_arg_t *arg)
{
	switch (arg->rettype) {
		case XMMS_OBJECT_CMD_ARG_STRING:
			xmms_ipc_msg_put_string (msg, arg->retval.string); /*convert to utf8?*/
			break;
		case XMMS_OBJECT_CMD_ARG_UINT32:
			xmms_ipc_msg_put_uint32 (msg, arg->retval.uint32);
			break;
		case XMMS_OBJECT_CMD_ARG_INT32:
			xmms_ipc_msg_put_int32 (msg, arg->retval.int32);
			break;
		case XMMS_OBJECT_CMD_ARG_STRINGLIST:
			{
				GList *l = arg->retval.stringlist;

				while (l) {
					xmms_ipc_msg_put_string (msg, l->data);
					l = g_list_delete_link (l, l);
				}
				break;
			}
		case XMMS_OBJECT_CMD_ARG_UINTLIST:
			{
				GList *l = arg->retval.uintlist;

				while (l) {
					xmms_ipc_msg_put_uint32 (msg, GPOINTER_TO_UINT (l->data));
					l = g_list_delete_link (l, l);
				}
				break;
			}
		case XMMS_OBJECT_CMD_ARG_INTLIST:
			{
				GList *l = arg->retval.intlist;

				while (l) {
					xmms_ipc_msg_put_int32 (msg, GPOINTER_TO_INT (l->data));
					l = g_list_delete_link (l, l);
				}
				break;
			}
		case XMMS_OBJECT_CMD_ARG_HASHLIST:
			{
				GList *l = arg->retval.hashlist;

				while (l) {
					if (l->data) {
						xmms_ipc_do_hashtable (msg, (GHashTable *)l->data);
						g_hash_table_destroy (l->data);
					}
					l = g_list_delete_link (l,l);
				}
				break;
			}
		case XMMS_OBJECT_CMD_ARG_PLCH:
			{
				xmms_playlist_changed_msg_t *chmsg = arg->retval.plch;
				xmms_ipc_handle_playlist_chmsg (msg, chmsg);
			}
			break;
		case XMMS_OBJECT_CMD_ARG_HASHTABLE: 
			{
				if (arg->retval.hashtable)  {
					xmms_ipc_do_hashtable (msg, arg->retval.hashtable);
				}

				break;
			}
		case XMMS_OBJECT_CMD_ARG_NONE:
			break;
		default:
			xmms_log_error ("Unknown returnvalue: %d, couldn't serialize message", arg->rettype);
			break;
	}
}

static void
process_msg (xmms_ipc_client_t *client, xmms_ipc_t *ipc, xmms_ipc_msg_t *msg)
{
	xmms_object_t *object;
	xmms_object_cmd_desc_t *cmd;
	xmms_object_cmd_arg_t arg;
	xmms_ipc_msg_t *retmsg;

	g_return_if_fail (ipc);
	g_return_if_fail (msg);

	if (msg->object == XMMS_IPC_OBJECT_SIGNAL && msg->cmd == XMMS_IPC_CMD_SIGNAL) {
		guint signalid;

		if (!xmms_ipc_msg_get_uint32 (msg, &signalid)) {
			XMMS_DBG ("No signalid in this msg?!");
			return;
		}

		g_mutex_lock (global_ipc_lock);
		client->pendingsignals[signalid] = msg->cid;
		g_mutex_unlock (global_ipc_lock);
		return;
	} else if (msg->object == XMMS_IPC_OBJECT_SIGNAL && msg->cmd == XMMS_IPC_CMD_BROADCAST) {
		guint broadcastid;

		if (!xmms_ipc_msg_get_uint32 (msg, &broadcastid)) {
			XMMS_DBG ("No broadcastid in this msg?!");
			return;
		}
		g_mutex_lock (global_ipc_lock);
		client->broadcasts[broadcastid] = msg->cid;
		g_mutex_unlock (global_ipc_lock);
		return;
	}

	XMMS_DBG ("Executing %d/%d", msg->object, msg->cmd);

	g_mutex_lock (global_ipc_lock);
	object = ipc->objects[msg->object];
	if (!object) {
		XMMS_DBG ("Object %d was not found!", msg->object);
		g_mutex_unlock (global_ipc_lock);
		return;
	}

	cmd = object->cmds[msg->cmd];
	if (!cmd) {
		XMMS_DBG ("No such cmd %d on object %d", msg->cmd, msg->object);
		g_mutex_unlock (global_ipc_lock);
		return;
	}
	g_mutex_unlock (global_ipc_lock);


        memset (&arg, 0, sizeof (arg));
        xmms_error_reset (&arg.error);

	if (cmd->arg1) {
		type_and_msg_to_arg (cmd->arg1, msg, &arg, 0);
	}

	if (cmd->arg2) {
		type_and_msg_to_arg (cmd->arg2, msg, &arg, 1);
	}

	xmms_object_cmd_call (object, msg->cmd, &arg);
	if (xmms_error_isok (&arg.error)) {
		retmsg = xmms_ipc_msg_new (msg->object, XMMS_IPC_CMD_REPLY);
		xmms_ipc_handle_arg_value (retmsg, &arg);
	} else {
		retmsg = xmms_ipc_msg_new (msg->object, XMMS_IPC_CMD_ERROR);
		xmms_ipc_msg_put_string (retmsg, xmms_error_message_get (&arg.error));
	}

	retmsg->cid = msg->cid;
	xmms_ipc_client_msg_write (client, retmsg);

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

			if (!client)
				continue;

			g_mutex_lock (client->mutex);
			xmms_ipc_client_ref (client);
			do {
				msg = g_queue_pop_head (client->msgs);

				if (!msg) 
					break;

				g_mutex_unlock (client->mutex);
				process_msg (client, ipc, msg);
				g_mutex_lock (client->mutex);

				xmms_ipc_msg_destroy (msg);
				xmms_ipc_client_unref (client); /* we give back the ref that the message reader took */
			} while (msg);
			g_mutex_unlock (client->mutex);
			xmms_ipc_client_unref (client);
		}

		g_cond_wait (ipc->msg_cond, ipc->mutex);
		
	}
	g_mutex_unlock (ipc->mutex);

	return NULL;
}

static void
xmms_ipc_signal_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata)
{
	GList *c;
	guint signalid = GPOINTER_TO_UINT (userdata);

	g_mutex_lock (global_ipc_lock);

	for (c = global_ipc->clients; c; c = g_list_next (c)) {
		xmms_ipc_client_t *cli = c->data;
		if (cli->pendingsignals[signalid]) {
			xmms_ipc_msg_t *msg;
			

			msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);
			msg->cid = cli->pendingsignals[signalid];
			xmms_ipc_handle_arg_value (msg, (xmms_object_cmd_arg_t*)arg);
			xmms_ipc_client_msg_write (cli, msg);
			xmms_ipc_msg_destroy (msg);
			cli->pendingsignals[signalid] = 0;
		}
	}

	g_mutex_unlock (global_ipc_lock);

}

static void
xmms_ipc_broadcast_cb (xmms_object_t *object, gconstpointer arg, gpointer userdata)
{
	GList *c;
	guint broadcastid = GPOINTER_TO_UINT (userdata);

	g_mutex_lock (global_ipc_lock);

	for (c = global_ipc->clients; c; c = g_list_next (c)) {
		xmms_ipc_client_t *cli = c->data;
		if (cli->broadcasts[broadcastid]) {
			xmms_ipc_msg_t *msg;


			msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_BROADCAST);
			msg->cid = cli->broadcasts[broadcastid];
			xmms_ipc_handle_arg_value (msg, (xmms_object_cmd_arg_t*)arg);
			xmms_ipc_client_msg_write (cli, msg);
			xmms_ipc_msg_destroy (msg);
		}
	}

	g_mutex_unlock (global_ipc_lock);
}

void
xmms_ipc_broadcast_register (xmms_object_t *object, xmms_ipc_signals_t signalid)
{
	xmms_object_t *obj;

	g_return_if_fail (object);

	g_mutex_lock (global_ipc_lock);

	obj = global_ipc->broadcasts[signalid];
	if (obj) {
		xmms_object_unref (obj);
	}

	xmms_object_ref (object);
	global_ipc->broadcasts[signalid] = object;

	xmms_object_connect (object, signalid, xmms_ipc_broadcast_cb, GUINT_TO_POINTER (signalid));

	g_mutex_unlock (global_ipc_lock);
}

void
xmms_ipc_broadcast_unregister (xmms_object_t *object, xmms_ipc_signals_t signalid)
{
	xmms_object_t *obj;

	g_return_if_fail (object);
	g_mutex_lock (global_ipc_lock);

	obj = global_ipc->broadcasts[signalid];
	if (obj) {
		xmms_object_disconnect (obj, signalid, xmms_ipc_broadcast_cb);
		xmms_object_unref (obj);
		global_ipc->broadcasts[signalid] = NULL;
	}

	g_mutex_unlock (global_ipc_lock);
}

void
xmms_ipc_signal_register (xmms_object_t *object, xmms_ipc_signals_t signalid)
{
	xmms_object_t *obj;

	g_return_if_fail (object);

	g_mutex_lock (global_ipc_lock);

	obj = global_ipc->signals[signalid];
	if (obj) {
		xmms_object_unref (obj);
	}

	xmms_object_ref (object);
	global_ipc->signals[signalid] = object;

	xmms_object_connect (object, signalid, xmms_ipc_signal_cb, GUINT_TO_POINTER (signalid));

	g_mutex_unlock (global_ipc_lock);
}

void
xmms_ipc_signal_unregister (xmms_object_t *object, xmms_ipc_signals_t signalid)
{
	xmms_object_t *obj;

	g_return_if_fail (object);
	g_mutex_lock (global_ipc_lock);

	obj = global_ipc->signals[signalid];
	if (obj) {
		xmms_object_disconnect (obj, signalid, xmms_ipc_signal_cb);
		xmms_object_unref (obj);
		global_ipc->signals[signalid] = NULL;
	}

	g_mutex_unlock (global_ipc_lock);
}

void
xmms_ipc_object_register (xmms_ipc_objects_t objectid, xmms_object_t *object)
{
	XMMS_DBG ("REGISTERING: '%d'", objectid);

	xmms_object_ref (object);

	g_mutex_lock (global_ipc_lock);
	if (global_ipc->objects[objectid]) {
		xmms_object_unref (global_ipc->objects[objectid]);
	}
	global_ipc->objects[objectid] = object;
	g_mutex_unlock (global_ipc_lock);

}

void
xmms_ipc_object_unregister (xmms_ipc_objects_t objectid)
{
	XMMS_DBG ("UNREGISTERING: '%d'", objectid);

	g_mutex_lock (global_ipc_lock);
	if (global_ipc->objects[objectid]) {
		xmms_object_unref (global_ipc->objects[objectid]);
		global_ipc->objects[objectid] = NULL;
	}
	g_mutex_unlock (global_ipc_lock);
}


xmms_ipc_t *
xmms_ipc_init (void)
{
	xmms_ipc_t *ipc;
	
	ipc = g_new0 (xmms_ipc_t, 1);
	ipc->mutex = g_mutex_new ();
	ipc->msg_cond = g_cond_new ();
	ipc->run = TRUE;

	XMMS_DBG ("IPC Initialized with %d CMDs!", XMMS_IPC_CMD_END);

	global_ipc_lock = g_mutex_new ();
	global_ipc = ipc;

	return ipc;
}

gboolean
xmms_ipc_setup_server (gchar *path)
{
	xmms_ipc_transport_t *transport; 

	g_return_val_if_fail (path, FALSE);
	g_return_val_if_fail (global_ipc, FALSE);
	
	transport = xmms_ipc_server_init (path);
	if (!transport) {
		XMMS_DBG ("THE FAIL!");
		return FALSE;
	}
	global_ipc->transport = transport;

	XMMS_DBG ("Starting ipc thread!");
	g_thread_create (xmms_ipc_handle_thread, global_ipc, FALSE, NULL);

	return TRUE;
}

