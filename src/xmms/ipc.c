/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <glib.h>
#include <string.h>

#include <xmms/xmms_log.h>
#include <xmms/xmms_config.h>
#include <xmmspriv/xmms_thread_name.h>
#include <xmmspriv/xmms_ipc.h>
#include <xmmsc/xmmsc_ipc_msg.h>


/**
  * @defgroup IPC IPC
  * @ingroup XMMSServer
  * @brief IPC functions for XMMS2 Daemon
  * @{
  */



/**
 * The IPC object list
 */
typedef struct xmms_ipc_object_pool_t {
	xmms_object_t *objects[XMMS_IPC_OBJECT_END];
	xmms_object_t *signals[XMMS_IPC_SIGNAL_END];
	xmms_object_t *broadcasts[XMMS_IPC_SIGNAL_END];
} xmms_ipc_object_pool_t;


/**
 * The server IPC object
 */
struct xmms_ipc_St {
	xmms_ipc_transport_t *transport;
	GList *clients;
	GIOChannel *chan;
	GMutex *mutex_lock;
	xmms_object_t **objects;
	xmms_object_t **signals;
	xmms_object_t **broadcasts;
};


/**
 * A IPC client representation.
 */
typedef struct xmms_ipc_client_St {
	GMainLoop *ml;
	GIOChannel *iochan;

	xmms_ipc_transport_t *transport;
	xmms_ipc_msg_t *read_msg;
	xmms_ipc_t *ipc;

	/* this lock protects out_msg, pendingsignals and broadcasts,
	   which can be accessed from other threads than the
	   client-thread */
	GMutex *lock;

	/** Messages waiting to be written */
	GQueue *out_msg;

	guint pendingsignals[XMMS_IPC_SIGNAL_END];
	GList *broadcasts[XMMS_IPC_SIGNAL_END];
} xmms_ipc_client_t;

static GMutex *ipc_servers_lock;
static GList *ipc_servers = NULL;

static GMutex *ipc_object_pool_lock;
static struct xmms_ipc_object_pool_t *ipc_object_pool = NULL;

static void xmms_ipc_client_destroy (xmms_ipc_client_t *client);

static void xmms_ipc_register_signal (xmms_ipc_client_t *client, xmms_ipc_msg_t *msg, xmmsv_t *arguments);
static void xmms_ipc_register_broadcast (xmms_ipc_client_t *client, xmms_ipc_msg_t *msg, xmmsv_t *arguments);
static gboolean xmms_ipc_client_msg_write (xmms_ipc_client_t *client, xmms_ipc_msg_t *msg);

static void
xmms_ipc_handle_cmd_value (xmms_ipc_msg_t *msg, xmmsv_t *val)
{
	if (xmms_ipc_msg_put_value (msg, val) == (uint32_t) -1) {
		xmms_log_error ("Failed to serialize the return value into the IPC message!");
	}
}

static void
xmms_ipc_register_signal (xmms_ipc_client_t *client,
                          xmms_ipc_msg_t *msg, xmmsv_t *arguments)
{
	xmmsv_t *arg;
	gint32 signalid;
	int r;

	if (!arguments || !xmmsv_list_get (arguments, 0, &arg)) {
		xmms_log_error ("No signalid in this msg?!");
		return;
	}

	r = xmmsv_get_int (arg, &signalid);

	if (!r) {
		xmms_log_error ("Cannot extract signal id from value");
		return;
	}

	if (signalid < 0 || signalid >= XMMS_IPC_SIGNAL_END) {
		xmms_log_error ("Bad signal id (%d)", signalid);
		return;
	}

	g_mutex_lock (client->lock);
	client->pendingsignals[signalid] = xmms_ipc_msg_get_cookie (msg);
	g_mutex_unlock (client->lock);
}

static void
xmms_ipc_register_broadcast (xmms_ipc_client_t *client,
                             xmms_ipc_msg_t *msg, xmmsv_t *arguments)
{
	xmmsv_t *arg;
	gint32 broadcastid;
	int r;

	if (!arguments || !xmmsv_list_get (arguments, 0, &arg)) {
		xmms_log_error ("No broadcastid in this msg?!");
		return;
	}

	r = xmmsv_get_int (arg, &broadcastid);

	if (!r) {
		xmms_log_error ("Cannot extract broadcast id from value");
		return;
	}

	if (broadcastid < 0 || broadcastid >= XMMS_IPC_SIGNAL_END) {
		xmms_log_error ("Bad broadcast id (%d)", broadcastid);
		return;
	}

	g_mutex_lock (client->lock);
	client->broadcasts[broadcastid] =
		g_list_append (client->broadcasts[broadcastid],
				GUINT_TO_POINTER (xmms_ipc_msg_get_cookie (msg)));

	g_mutex_unlock (client->lock);
}

static void
process_msg (xmms_ipc_client_t *client, xmms_ipc_msg_t *msg)
{
	xmms_object_t *object;
	xmms_object_cmd_arg_t arg;
	xmms_ipc_msg_t *retmsg;
	xmmsv_t *error, *arguments;
	uint32_t objid, cmdid;

	g_return_if_fail (msg);

	objid = xmms_ipc_msg_get_object (msg);
	cmdid = xmms_ipc_msg_get_cmd (msg);

	if (!xmms_ipc_msg_get_value (msg, &arguments)) {
		xmms_log_error ("Cannot read command arguments. "
		                "Ignoring command.");

		return;
	}

	if (objid == XMMS_IPC_OBJECT_SIGNAL) {
	    if (cmdid == XMMS_IPC_CMD_SIGNAL) {
			xmms_ipc_register_signal (client, msg, arguments);
		} else if (cmdid == XMMS_IPC_CMD_BROADCAST) {
			xmms_ipc_register_broadcast (client, msg, arguments);
		} else {
			xmms_log_error ("Bad command id (%d) for signal object", cmdid);
		}

		goto out;
	}

	if (objid >= XMMS_IPC_OBJECT_END) {
		xmms_log_error ("Bad object id (%d)", objid);
		goto out;
	}

	g_mutex_lock (ipc_object_pool_lock);
	object = ipc_object_pool->objects[objid];
	g_mutex_unlock (ipc_object_pool_lock);
	if (!object) {
		xmms_log_error ("Object %d was not found!", objid);
		goto out;
	}

	if (!g_tree_lookup (object->cmds, GUINT_TO_POINTER (cmdid))) {
		xmms_log_error ("No such cmd %d on object %d", cmdid, objid);
		goto out;
	}

	xmms_object_cmd_arg_init (&arg);
	arg.args = arguments;

	xmms_object_cmd_call (object, cmdid, &arg);
	if (xmms_error_isok (&arg.error)) {
		retmsg = xmms_ipc_msg_new (objid, XMMS_IPC_CMD_REPLY);
		xmms_ipc_handle_cmd_value (retmsg, arg.retval);
	} else {
		/* FIXME: or we could omit setting the command to _CMD_ERROR
		 * and let the client check whether the value it got is an
		 * error xmmsv_t. If so, don't forget to
		 * update the client-side of IPC too. */
		retmsg = xmms_ipc_msg_new (objid, XMMS_IPC_CMD_ERROR);

		error = xmmsv_new_error (xmms_error_message_get (&arg.error));
		xmms_ipc_msg_put_value (retmsg, error);
		xmmsv_unref (error);

/*
		retmsg = xmms_ipc_msg_new (objid, XMMS_IPC_CMD_REPLY);
		xmms_ipc_handle_cmd_value (retmsg, arg.retval);
*/
	}

	if (arg.retval)
		xmmsv_unref (arg.retval);

	xmms_ipc_msg_set_cookie (retmsg, xmms_ipc_msg_get_cookie (msg));
	g_mutex_lock (client->lock);
	xmms_ipc_client_msg_write (client, retmsg);
	g_mutex_unlock (client->lock);

out:
	if (arguments) {
		xmmsv_unref (arguments);
	}
}


static gboolean
xmms_ipc_client_read_cb (GIOChannel *iochan,
                         GIOCondition cond,
                         gpointer data)
{
	xmms_ipc_client_t *client = data;
	bool disconnect = FALSE;

	g_return_val_if_fail (client, FALSE);

	if (cond & G_IO_IN) {
		while (TRUE) {
			if (!client->read_msg) {
				client->read_msg = xmms_ipc_msg_alloc ();
			}

			if (xmms_ipc_msg_read_transport (client->read_msg, client->transport, &disconnect)) {
				xmms_ipc_msg_t *msg = client->read_msg;
				client->read_msg = NULL;
				process_msg (client, msg);
				xmms_ipc_msg_destroy (msg);
			} else {
				break;
			}
		}
	}

	if (disconnect || (cond & G_IO_HUP)) {
		if (client->read_msg) {
			xmms_ipc_msg_destroy (client->read_msg);
			client->read_msg = NULL;
		}
		XMMS_DBG ("disconnect was true!");
		g_main_loop_quit (client->ml);
		return FALSE;
	}

	if (cond & G_IO_ERR) {
		xmms_log_error ("Client got error, maybe connection died?");
		g_main_loop_quit (client->ml);
		return FALSE;
	}

	return TRUE;
}

static gboolean
xmms_ipc_client_write_cb (GIOChannel *iochan,
                          GIOCondition cond,
                          gpointer data)
{
	xmms_ipc_client_t *client = data;
	bool disconnect = FALSE;

	g_return_val_if_fail (client, FALSE);

	while (TRUE) {
		xmms_ipc_msg_t *msg;

		g_mutex_lock (client->lock);
		msg = g_queue_peek_head (client->out_msg);
		g_mutex_unlock (client->lock);

		if (!msg)
			break;

		if (!xmms_ipc_msg_write_transport (msg,
		                                   client->transport,
		                                   &disconnect)) {
			if (disconnect) {
				break;
			} else {
				/* try sending again later */
				return TRUE;
			}
		}

		g_mutex_lock (client->lock);
		g_queue_pop_head (client->out_msg);
		g_mutex_unlock (client->lock);

		xmms_ipc_msg_destroy (msg);
	}

	return FALSE;
}

static gpointer
xmms_ipc_client_thread (gpointer data)
{
	xmms_ipc_client_t *client = data;
	GSource *source;

	xmms_set_thread_name ("x2 client");

	source = g_io_create_watch (client->iochan, G_IO_IN | G_IO_ERR | G_IO_HUP);
	g_source_set_callback (source,
	                       (GSourceFunc) xmms_ipc_client_read_cb,
	                       (gpointer) client,
	                       NULL);
	g_source_attach (source, g_main_loop_get_context (client->ml));
	g_source_unref (source);

	g_main_loop_run (client->ml);

	xmms_ipc_client_destroy (client);

	return NULL;
}

static xmms_ipc_client_t *
xmms_ipc_client_new (xmms_ipc_t *ipc, xmms_ipc_transport_t *transport)
{
	xmms_ipc_client_t *client;
	GMainContext *context;
	int fd;

	g_return_val_if_fail (transport, NULL);

	client = g_new0 (xmms_ipc_client_t, 1);

	context = g_main_context_new ();
	client->ml = g_main_loop_new (context, FALSE);
	g_main_context_unref (context);

	fd = xmms_ipc_transport_fd_get (transport);
	client->iochan = g_io_channel_unix_new (fd);
	g_return_val_if_fail (client->iochan, NULL);

	/* We don't set the close_on_unref flag here, because
	 * the transport will close the fd for us. No need to close it twice.
	 */
	g_io_channel_set_encoding (client->iochan, NULL, NULL);
	g_io_channel_set_buffered (client->iochan, FALSE);

	client->transport = transport;
	client->ipc = ipc;
	client->out_msg = g_queue_new ();
	client->lock = g_mutex_new ();

	return client;
}

static void
xmms_ipc_client_destroy (xmms_ipc_client_t *client)
{
	guint i;

	XMMS_DBG ("Destroying client!");

	if (client->ipc) {
		g_mutex_lock (client->ipc->mutex_lock);
		client->ipc->clients = g_list_remove (client->ipc->clients, client);
		g_mutex_unlock (client->ipc->mutex_lock);
	}

	g_main_loop_unref (client->ml);
	g_io_channel_unref (client->iochan);

	xmms_ipc_transport_destroy (client->transport);

	g_mutex_lock (client->lock);
	while (!g_queue_is_empty (client->out_msg)) {
		xmms_ipc_msg_t *msg = g_queue_pop_head (client->out_msg);
		xmms_ipc_msg_destroy (msg);
	}

	g_queue_free (client->out_msg);

	for (i = 0; i < XMMS_IPC_SIGNAL_END; i++) {
		g_list_free (client->broadcasts[i]);
	}

	g_mutex_unlock (client->lock);
	g_mutex_free (client->lock);
	g_free (client);
}

/**
 * Gets called when the config property "core.ipcsocket" has changed.
 */
void
on_config_ipcsocket_change (xmms_object_t *object, xmmsv_t *_data, gpointer udata)
{
	const gchar *value;

	XMMS_DBG ("Shutting down ipc server threads through config property \"core.ipcsocket\" change.");

	xmms_ipc_shutdown ();
	value = xmms_config_property_get_string ((xmms_config_property_t *) object);
	xmms_ipc_setup_server (value);
}

/**
 * Put a message in the queue awaiting to be sent to the client.
 * Should hold client->lock.
 */
static gboolean
xmms_ipc_client_msg_write (xmms_ipc_client_t *client, xmms_ipc_msg_t *msg)
{
	gboolean queue_empty;

	g_return_val_if_fail (client, FALSE);
	g_return_val_if_fail (msg, FALSE);

	queue_empty = g_queue_is_empty (client->out_msg);
	g_queue_push_tail (client->out_msg, msg);

	/* If there's no write in progress, add a new callback */
	if (queue_empty) {
		GMainContext *context = g_main_loop_get_context (client->ml);
		GSource *source = g_io_create_watch (client->iochan, G_IO_OUT);

		g_source_set_callback (source,
		                       (GSourceFunc) xmms_ipc_client_write_cb,
		                       (gpointer) client,
		                       NULL);
		g_source_attach (source, context);
		g_source_unref (source);

		g_main_context_wakeup (context);
	}

	return TRUE;
}

static gboolean
xmms_ipc_source_accept (GIOChannel *chan, GIOCondition cond, gpointer data)
{
	xmms_ipc_t *ipc = (xmms_ipc_t *) data;
	xmms_ipc_transport_t *transport;
	xmms_ipc_client_t *client;

	if (!(cond & G_IO_IN)) {
		xmms_log_error ("IPC listener got error/hup");
		return FALSE;
	}

	XMMS_DBG ("Client connected");
	transport = xmms_ipc_server_accept (ipc->transport);
	if (!transport) {
		xmms_log_error ("accept returned null!");
		return TRUE;
	}

	client = xmms_ipc_client_new (ipc, transport);
	if (!client) {
		xmms_ipc_transport_destroy (transport);
		return TRUE;
	}

	g_mutex_lock (ipc->mutex_lock);
	ipc->clients = g_list_append (ipc->clients, client);
	g_mutex_unlock (ipc->mutex_lock);

	/* Now that the client has been registered in the ipc->clients list
	 * we may safely start its thread.
	 */
	g_thread_create (xmms_ipc_client_thread, client, FALSE, NULL);

	return TRUE;
}

/**
 * Enable IPC
 */
static gboolean
xmms_ipc_setup_server_internaly (xmms_ipc_t *ipc)
{
	g_mutex_lock (ipc->mutex_lock);
	ipc->chan = g_io_channel_unix_new (xmms_ipc_transport_fd_get (ipc->transport));

	g_io_channel_set_close_on_unref (ipc->chan, TRUE);
	g_io_channel_set_encoding (ipc->chan, NULL, NULL);
	g_io_channel_set_buffered (ipc->chan, FALSE);

	g_io_add_watch (ipc->chan, G_IO_IN | G_IO_HUP | G_IO_ERR,
	                xmms_ipc_source_accept, ipc);
	g_mutex_unlock (ipc->mutex_lock);
	return TRUE;
}

/**
 * Checks if someone is waiting for signalid
 */
gboolean
xmms_ipc_has_pending (guint signalid)
{
	GList *c, *s;
	xmms_ipc_t *ipc;

	g_mutex_lock (ipc_servers_lock);

	for (s = ipc_servers; s; s = g_list_next (s)) {
		ipc = s->data;
		g_mutex_lock (ipc->mutex_lock);
		for (c = ipc->clients; c; c = g_list_next (c)) {
			xmms_ipc_client_t *cli = c->data;
			g_mutex_lock (cli->lock);
			if (cli->pendingsignals[signalid]) {
				g_mutex_unlock (cli->lock);
				g_mutex_unlock (ipc->mutex_lock);
				g_mutex_unlock (ipc_servers_lock);
				return TRUE;
			}
			g_mutex_unlock (cli->lock);
		}
		g_mutex_unlock (ipc->mutex_lock);
	}

	g_mutex_unlock (ipc_servers_lock);
	return FALSE;
}

static void
xmms_ipc_signal_cb (xmms_object_t *object, xmmsv_t *arg, gpointer userdata)
{
	GList *c, *s;
	guint signalid = GPOINTER_TO_UINT (userdata);
	xmms_ipc_t *ipc;
	xmms_ipc_msg_t *msg;

	g_mutex_lock (ipc_servers_lock);

	for (s = ipc_servers; s && s->data; s = g_list_next (s)) {
		ipc = s->data;
		g_mutex_lock (ipc->mutex_lock);
		for (c = ipc->clients; c; c = g_list_next (c)) {
			xmms_ipc_client_t *cli = c->data;
			g_mutex_lock (cli->lock);
			if (cli->pendingsignals[signalid]) {
				msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);
				xmms_ipc_msg_set_cookie (msg, cli->pendingsignals[signalid]);
				xmms_ipc_handle_cmd_value (msg, arg);
				xmms_ipc_client_msg_write (cli, msg);
				cli->pendingsignals[signalid] = 0;
			}
			g_mutex_unlock (cli->lock);
		}
		g_mutex_unlock (ipc->mutex_lock);
	}

	g_mutex_unlock (ipc_servers_lock);

}

static void
xmms_ipc_broadcast_cb (xmms_object_t *object, xmmsv_t *arg, gpointer userdata)
{
	GList *c, *s;
	guint broadcastid = GPOINTER_TO_UINT (userdata);
	xmms_ipc_t *ipc;
	xmms_ipc_msg_t *msg = NULL;
	GList *l;

	g_mutex_lock (ipc_servers_lock);

	for (s = ipc_servers; s && s->data; s = g_list_next (s)) {
		ipc = s->data;
		g_mutex_lock (ipc->mutex_lock);
		for (c = ipc->clients; c; c = g_list_next (c)) {
			xmms_ipc_client_t *cli = c->data;

			g_mutex_lock (cli->lock);
			for (l = cli->broadcasts[broadcastid]; l; l = g_list_next (l)) {
				msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_BROADCAST);
				xmms_ipc_msg_set_cookie (msg, GPOINTER_TO_UINT (l->data));
				xmms_ipc_handle_cmd_value (msg, arg);
				xmms_ipc_client_msg_write (cli, msg);
			}
			g_mutex_unlock (cli->lock);
		}
		g_mutex_unlock (ipc->mutex_lock);
	}
	g_mutex_unlock (ipc_servers_lock);
}

/**
 * Register a broadcast signal.
 */
void
xmms_ipc_broadcast_register (xmms_object_t *object, xmms_ipc_signals_t signalid)
{
	g_return_if_fail (object);
	g_mutex_lock (ipc_object_pool_lock);

	ipc_object_pool->broadcasts[signalid] = object;
	xmms_object_connect (object, signalid, xmms_ipc_broadcast_cb, GUINT_TO_POINTER (signalid));

	g_mutex_unlock (ipc_object_pool_lock);
}

/**
 * Unregister a broadcast signal.
 */
void
xmms_ipc_broadcast_unregister (xmms_ipc_signals_t signalid)
{
	xmms_object_t *obj;

	g_mutex_lock (ipc_object_pool_lock);
	obj = ipc_object_pool->broadcasts[signalid];
	if (obj) {
		xmms_object_disconnect (obj, signalid, xmms_ipc_broadcast_cb, GUINT_TO_POINTER (signalid));
		ipc_object_pool->broadcasts[signalid] = NULL;
	}
	g_mutex_unlock (ipc_object_pool_lock);
}

/**
 * Register a signal
 */
void
xmms_ipc_signal_register (xmms_object_t *object, xmms_ipc_signals_t signalid)
{
	g_return_if_fail (object);

	g_mutex_lock (ipc_object_pool_lock);
	ipc_object_pool->signals[signalid] = object;
	xmms_object_connect (object, signalid, xmms_ipc_signal_cb, GUINT_TO_POINTER (signalid));
	g_mutex_unlock (ipc_object_pool_lock);
}

/**
 * Unregister a signal
 */
void
xmms_ipc_signal_unregister (xmms_ipc_signals_t signalid)
{
	xmms_object_t *obj;

	g_mutex_lock (ipc_object_pool_lock);
	obj = ipc_object_pool->signals[signalid];
	if (obj) {
		xmms_object_disconnect (obj, signalid, xmms_ipc_signal_cb, GUINT_TO_POINTER (signalid));
		ipc_object_pool->signals[signalid] = NULL;
	}
	g_mutex_unlock (ipc_object_pool_lock);
}

/**
 * Register a object to the IPC core. This needs to be done if you
 * want to send commands to that object from the client.
 */
void
xmms_ipc_object_register (xmms_ipc_objects_t objectid, xmms_object_t *object)
{
	g_mutex_lock (ipc_object_pool_lock);
	ipc_object_pool->objects[objectid] = object;
	g_mutex_unlock (ipc_object_pool_lock);
}

/**
 * Remove a object from the IPC core.
 */
void
xmms_ipc_object_unregister (xmms_ipc_objects_t objectid)
{
	g_mutex_lock (ipc_object_pool_lock);
	ipc_object_pool->objects[objectid] = NULL;
	g_mutex_unlock (ipc_object_pool_lock);
}

/**
 * Initialize IPC
 */
xmms_ipc_t *
xmms_ipc_init (void)
{
	ipc_servers_lock = g_mutex_new ();
	ipc_object_pool_lock = g_mutex_new ();
	ipc_object_pool = g_new0 (xmms_ipc_object_pool_t, 1);
	return NULL;
}

/**
 * Shutdown a IPC Server
 */
static void
xmms_ipc_shutdown_server (xmms_ipc_t *ipc)
{
	GList *c;
	xmms_ipc_client_t *co;
	if (!ipc) return;

	g_mutex_lock (ipc->mutex_lock);
	g_source_remove_by_user_data (ipc);
	g_io_channel_unref (ipc->chan);
	xmms_ipc_transport_destroy (ipc->transport);

	for (c = ipc->clients; c; c = g_list_next (c)) {
		co = c->data;
		if (!co) continue;
		co->ipc = NULL;
	}

	g_list_free (ipc->clients);
	g_mutex_unlock (ipc->mutex_lock);
	g_mutex_free (ipc->mutex_lock);

	g_free (ipc);

}


/**
 * Disable IPC
 */
void
xmms_ipc_shutdown (void)
{
	GList *s = ipc_servers;
	xmms_ipc_t *ipc;

	g_mutex_lock (ipc_servers_lock);
	while (s) {
		ipc = s->data;
		s = g_list_next (s);
		ipc_servers = g_list_remove (ipc_servers, ipc);
		xmms_ipc_shutdown_server (ipc);
	}
	g_mutex_unlock (ipc_servers_lock);

	g_mutex_free (ipc_servers_lock);
	ipc_servers_lock = NULL;

	g_mutex_free (ipc_object_pool_lock);
	ipc_object_pool_lock = NULL;

	g_free (ipc_object_pool);
	ipc_object_pool = NULL;
}

/**
 * Start the server
 */
gboolean
xmms_ipc_setup_server (const gchar *path)
{
	xmms_ipc_transport_t *transport;
	xmms_ipc_t *ipc;
	gchar **split;
	gint i = 0, num_init = 0;
	g_return_val_if_fail (path, FALSE);

	split = g_strsplit (path, ";", 0);

	for (i = 0; split && split[i]; i++) {
		ipc = g_new0 (xmms_ipc_t, 1);
		if (!ipc) {
			XMMS_DBG ("No IPC server initialized.");
			continue;
		}

		transport = xmms_ipc_server_init (split[i]);
		if (!transport) {
			g_free (ipc);
			xmms_log_error ("Couldn't setup IPC listening on '%s'.", split[i]);
			continue;
		}


		ipc->mutex_lock = g_mutex_new ();
		ipc->transport = transport;
		ipc->signals = ipc_object_pool->signals;
		ipc->broadcasts = ipc_object_pool->broadcasts;
		ipc->objects = ipc_object_pool->objects;

		xmms_ipc_setup_server_internaly (ipc);
		xmms_log_info ("IPC listening on '%s'.", split[i]);

		g_mutex_lock (ipc_servers_lock);
		ipc_servers = g_list_prepend (ipc_servers, ipc);
		g_mutex_unlock (ipc_servers_lock);

		num_init++;
	}

	g_strfreev (split);


	/* If there is less than one socket, there is sth. wrong. */
	if (num_init < 1)
		return FALSE;

	XMMS_DBG ("IPC setup done.");
	return TRUE;
}

/** @} */

