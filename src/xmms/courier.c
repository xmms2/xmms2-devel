/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

/** @file
 * The courier object.
 * This object is responsible for forwarding messages between clients.
 */

#include <glib.h>

#include <xmms/xmms_log.h>
#include <xmms/xmms_object.h>

#include <xmmsc/xmmsc_ipc_msg.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmmscpriv/xmmsv_c2c.h>

#include <xmmspriv/xmms_ipc.h>
#include <xmmspriv/xmms_courier.h>

/**
 * Representation of a pending message: a client-to client message whose sender
 * is expecting a reply from the recipient.
 */
typedef struct {
	gint32 sender;
	gint32 destination;
	uint32_t cookie;
	xmms_c2c_reply_policy_t reply_policy;
} xmms_courier_pending_msg_t;

/**
 * The pool of pending messages.
 */
typedef struct {
	gint32 last_id;
	GMutex lock;
	GTree *messages;
} xmms_courier_pending_pool_t;

/**
 * The courier object.
 */
struct xmms_courier_St {
	xmms_object_t object;

	xmms_ipc_manager_t *manager;

	GList *clients;
	GList *ready;
	GMutex clients_lock;
	xmms_courier_pending_pool_t *pending_pool;
};

/* Initializers/destroyers */
static void xmms_courier_destroy (xmms_object_t *object);
static xmms_courier_pending_pool_t *xmms_courier_pending_pool_init (void);
static void xmms_courier_pending_pool_destroy (xmms_courier_pending_pool_t *pending);

/* Public methods */
static void xmms_courier_client_send_message (xmms_courier_t *courier, gint32 dest, int reply_policy, xmmsv_t *payload, gint32 client, uint32_t cookie, xmms_error_t *err);
static void xmms_courier_client_reply (xmms_courier_t *courier, gint32 msgid, int reply_policy, xmmsv_t *payload, gint32 sender, uint32_t cookie, xmms_error_t *err);
static xmmsv_t *xmms_courier_client_get_connected_clients (xmms_courier_t *courier, xmms_error_t *err);
static void xmms_courier_client_ready (xmms_courier_t *courier, gint32 clientid, xmms_error_t *err);
static xmmsv_t *xmms_courier_client_get_ready_clients (xmms_courier_t *courier, xmms_error_t *err);

/* Private methods */
static gint32 xmms_courier_store_pending (xmms_courier_t *courier, gint32 sender, gint32 dest, uint32_t cookie, xmms_c2c_reply_policy_t reply_policy);
static xmms_courier_pending_msg_t *xmms_courier_get_pending (xmms_courier_t *courier, gint32 msgid);
static void xmms_courier_remove_pending (xmms_courier_t *courier, gint32 msgid);

/* Helper functions */
static void write_reply (gint32 dest, xmmsv_t *c2c_msg, uint32_t cookie, xmms_error_t *err);
static gint compare_int32 (gconstpointer a, gconstpointer b, gpointer unused);
static void client_connected_cb (xmms_object_t *unused, xmmsv_t *val, gpointer userdata);
static void client_disconnected_cb (xmms_object_t *unused, xmmsv_t *val, gpointer userdata);
static gboolean client_disconnected_foreach (gpointer key, gpointer value, gpointer userdata);

#include "courier_ipc.c"

/**
 * Initialize a new xmms_courier_t.
 */
xmms_courier_t *
xmms_courier_init (void)
{
	xmms_courier_t *courier;

	courier = xmms_object_new (xmms_courier_t, xmms_courier_destroy);
	courier->pending_pool = xmms_courier_pending_pool_init ();
	g_mutex_init (&courier->clients_lock);

	courier->manager = xmms_ipc_manager_get();
	xmms_object_ref(courier->manager);

	xmms_courier_register_ipc_commands (XMMS_OBJECT (courier));

	xmms_object_connect (XMMS_OBJECT (courier->manager),
	                     XMMS_IPC_SIGNAL_IPC_MANAGER_CLIENT_CONNECTED,
	                     client_connected_cb,
	                     (gpointer) courier);

	xmms_object_connect (XMMS_OBJECT (courier->manager),
	                     XMMS_IPC_SIGNAL_IPC_MANAGER_CLIENT_DISCONNECTED,
	                     client_disconnected_cb,
	                     (gpointer) courier);

	return courier;
}

/**
 * Destroy a courier object.
 */
static void
xmms_courier_destroy (xmms_object_t *object)
{
	xmms_courier_t *courier;

	courier = (xmms_courier_t *) object;
	g_list_free (courier->clients);
	g_list_free (courier->ready);
	g_mutex_clear (&courier->clients_lock);
	xmms_courier_pending_pool_destroy (courier->pending_pool);

	xmms_courier_unregister_ipc_commands ();

	xmms_object_unref(courier->manager);

	return;
}

/**
 * Initialize a pool of pending messages.
 */
static xmms_courier_pending_pool_t *
xmms_courier_pending_pool_init (void)
{
	xmms_courier_pending_pool_t *ret;

	ret = g_new0 (xmms_courier_pending_pool_t, 1);
	g_mutex_init (&ret->lock);
	ret->messages = g_tree_new_full (compare_int32, NULL, NULL, g_free);

	return ret;
}

/**
 * Destroy a pool of pending messages.
 */
static void
xmms_courier_pending_pool_destroy (xmms_courier_pending_pool_t *pending)
{
	/* Free the pool of pending messages */
	g_mutex_clear (&pending->lock);
	g_tree_destroy (pending->messages);
	g_free (pending);

	return;
}

/**
 * Send a message.
 *
 * @param courier the courier object
 * @param reply_policy the reply policy expected by the sender
 * @param c2c_msg a c2c-message (with at least msgid, sender, destination)
 * @param is_reply whether a reply (intead of a normal message) is to be sent
 * @param dcookie in case is_reply is TRUE, the cookie associated with the
 *               message that is being replied to in the destination client
 * @param scookie in case is_reply is TRUE, the cookie associated with the
 *               message that is being replied to in the sender client
 *
 * @sa send_msg
 * @sa send_reply
 */
static void
send_internal (xmms_courier_t *courier,
               int reply_policy, xmmsv_t *c2c_msg,
               gboolean is_reply, uint32_t dcookie,
               uint32_t scookie, xmms_error_t *err)
{
	int msgid = xmmsv_c2c_message_get_id (c2c_msg);
	gint32 sender = xmmsv_c2c_message_get_sender (c2c_msg);
	gint32 dest = xmmsv_c2c_message_get_destination (c2c_msg);

	if (is_reply) {
		write_reply (dest, c2c_msg, dcookie, err);
	} else {
		xmms_ipc_send_broadcast (XMMS_IPC_SIGNAL_COURIER_MESSAGE,
		                         dest, c2c_msg, err);
	}

	if (xmms_error_isok (err)) {
		/* Reply NONE to the sender if it doesn't expect a reply. */
		if (reply_policy == XMMS_C2C_REPLY_POLICY_NO_REPLY) {
			xmmsv_t *none;
			none = xmmsv_new_none ();

			write_reply (sender, none, scookie, err);
			xmmsv_unref (none);
		}
	} else {
		/* Remove the entry from the pending pool upon error */
		if (reply_policy != XMMS_C2C_REPLY_POLICY_NO_REPLY) {
			xmms_courier_remove_pending (courier, msgid);
		}
	}

	return;
}

/**
 * Send a c2c-reply.
 * In case of an error an error-message is sent to the sender (and a possibly
 * expected reply (as per reply_policy) is cancelled). Otherwise, if
 * reply_policy is NO_REPLY, a NONE-message is sent back to the sender. If
 * reply_policy is something else, nothing is sent to the sender.
 *
 * @param courier the courier object
 * @param reply_policy the reply policy expected by the sender
 * @param c2c_msg a c2c-message (with at least msgid, sender, destination)
 * @param dcookie the cookie associated with the message that is being
 * replied to in the destination client.
 * @param scookie the cookie associated with the reply in the sender.
 */
static void
send_reply (xmms_courier_t *courier,
            int reply_policy, xmmsv_t *c2c_msg, uint32_t dcookie,
            uint32_t scookie, xmms_error_t *err)
{
	send_internal (courier, reply_policy, c2c_msg, TRUE, dcookie, scookie, err);
	return;
}

/**
 * Send a c2c-message.
 * In case of an error an error-message is sent to the sender (and a possibly
 * expected reply (as per reply_policy) is cancelled). Otherwise, if
 * reply_policy is NO_REPLY, a NONE-message is sent back to the sender. If
 * reply_policy is something else, nothing is sent to the sender.
 *
 * @param courier the courier object
 * @param reply_policy the reply policy expected by the sender
 * @param c2c_msg a c2c-message (with at least msgid, sender, destination)
 */
static void
send_msg (xmms_courier_t *courier,
          int reply_policy, xmmsv_t *c2c_msg,
          xmms_error_t *err)
{
	send_internal (courier, reply_policy, c2c_msg, FALSE, 0, 0, err);
	return;
}

/**
 * Assemble and send a client-to-client message.
 *
 * @param dest the id of the recipient client
 * @param reply_policy the reply policy expected by the sender
 * @param payload the contents of the message
 */
static void
xmms_courier_client_send_message (xmms_courier_t *courier,
                                  gint32 dest, int reply_policy,
                                  xmmsv_t *payload, gint32 sender,
                                  uint32_t cookie, xmms_error_t *err)
{
	gint32 msgid;
	xmmsv_t *c2c_msg;

	msgid = 0;

	if (reply_policy != XMMS_C2C_REPLY_POLICY_NO_REPLY) {
		msgid = xmms_courier_store_pending (courier, sender, dest, cookie,
		                                    reply_policy);
	}

	c2c_msg = xmmsv_c2c_message_format (sender, dest, msgid, payload);
	send_msg (courier, reply_policy, c2c_msg, err);
	xmmsv_unref (c2c_msg);

	return;
}

/**
 * Assemble and send a reply to a client-to-client message.
 *
 * @param reply_to the id of the message that is being replied to
 * @param reply_policy the reply policy expected by the sender
 * @param payload the contents of the reply
 */
static void
xmms_courier_client_reply (xmms_courier_t *courier, gint32 reply_to,
                           int reply_policy, xmmsv_t *payload, gint32 sender,
                           uint32_t cookie, xmms_error_t *err)
{
	xmmsv_t *c2c_msg;
	xmms_courier_pending_msg_t *context;
	gint32 dest;
	gint32 msgid;

	/* Restore the context of the original message from the pending pool */
	context = xmms_courier_get_pending (courier, reply_to);
	if (context == NULL) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "pending message not found");
		return;
	}

	/* Check if the answering client was the destination of the message */
	if (sender != context->destination) {
		xmms_error_set (err, XMMS_ERROR_PERMISSION, "sender mismatch in reply");
		return;
	}

	/* Send the message as usual */
	msgid = 0;
	dest = context->sender;
	if (reply_policy != XMMS_C2C_REPLY_POLICY_NO_REPLY) {
		msgid = xmms_courier_store_pending (courier, sender, dest, cookie,
		                                    reply_policy);
	}

	c2c_msg = xmmsv_c2c_message_format (sender, dest, msgid, payload);
	send_reply (courier, reply_policy, c2c_msg, context->cookie, cookie, err);
	xmmsv_unref (c2c_msg);

	if (context->reply_policy == XMMS_C2C_REPLY_POLICY_SINGLE_REPLY) {
		/* Remove the context if no more replies are expected. */
		xmms_courier_remove_pending (courier, reply_to);
	}

	/* May or not be invalid, don't use this anymore. */
	context = NULL;

	return;
}

/**
 * Get a list of ids of connected clients.
 */
static xmmsv_t *
xmms_courier_client_get_connected_clients (xmms_courier_t *courier,
                                           xmms_error_t *err)
{
	GList *next;
	xmmsv_t *ret;

	ret = xmmsv_new_list();
	g_mutex_lock (&courier->clients_lock);
	/* Build a list of xmmsv ints from the list of connected clients */
	for (next = courier->clients; next; next = g_list_next (next)) {
		xmmsv_list_append_int(ret, GPOINTER_TO_INT(next->data));
	}
	g_mutex_unlock (&courier->clients_lock);

	return ret;
}

/**
 * A client notifies its service api is ready for query.
 * Forward the notification to other clients.
 */
static void
xmms_courier_client_ready (xmms_courier_t *courier,
                           gint32 clientid, xmms_error_t *err)
{
	g_mutex_lock (&courier->clients_lock);
	if (g_list_index (courier->ready, GINT_TO_POINTER (clientid)) < 0) {
		courier->ready = g_list_prepend (courier->ready, GINT_TO_POINTER (clientid));
	}
	g_mutex_unlock (&courier->clients_lock);
	
	xmms_object_emit(XMMS_OBJECT(courier), XMMS_IPC_SIGNAL_COURIER_READY,
	                 xmmsv_new_int(clientid));
}

/**
 * Get a list of ids of clients ready for c2c communication.
 */
static xmmsv_t *
xmms_courier_client_get_ready_clients (xmms_courier_t *courier,
                                       xmms_error_t *err)
{
	GList *next;
	xmmsv_t *ret;

	ret = xmmsv_new_list();
	g_mutex_lock (&courier->clients_lock);
	/* Build a list of xmmsv ints from the list of connected clients */
	for (next = courier->ready; next; next = g_list_next (next)) {
		xmmsv_list_append_int(ret, GPOINTER_TO_INT(next->data));
	}
	g_mutex_unlock (&courier->clients_lock);

	return ret;
}

/**
 * Save the context of a pending client-to-client message.
 *
 * @param sender the id of the sender client
 * @param dest the id of the recipient client
 * @param cookie the cookie associated with the message in the sender client
 *
 * @return the id of the message
 */
static gint32
xmms_courier_store_pending (xmms_courier_t *courier, gint32 sender,
                            gint32 dest, uint32_t cookie,
                            xmms_c2c_reply_policy_t reply_policy)
{
	gint32 msgid;
	xmms_courier_pending_msg_t *entry;
	xmms_courier_pending_pool_t *pending = courier->pending_pool;

	entry = g_new (xmms_courier_pending_msg_t, 1);
	entry->sender = sender;
	entry->destination = dest;
	entry->cookie = cookie;
	entry->reply_policy = reply_policy;

	g_mutex_lock (&pending->lock);

	/* Register the next non-zero id available */
	msgid = ++pending->last_id;
	g_tree_insert (pending->messages, GINT_TO_POINTER (msgid),
	               (gpointer) entry);

	g_mutex_unlock (&pending->lock);

	XMMS_DBG ("Stored pending message %d", msgid);
	return msgid;
}

/**
 * Restore the context of a pending client-to-client message.
 *
 * @param msgid the id of the message
 */
static xmms_courier_pending_msg_t *
xmms_courier_get_pending (xmms_courier_t *courier, gint32 msgid)
{
	xmms_courier_pending_msg_t *msg;
	xmms_courier_pending_pool_t *pending = courier->pending_pool;

	g_mutex_lock (&pending->lock);
	msg = (xmms_courier_pending_msg_t *)
	      g_tree_lookup (pending->messages, GINT_TO_POINTER (msgid));
	g_mutex_unlock (&pending->lock);

	return msg;
}

/**
 * Remove the context of a pending client-to-client message.
 *
 * @param msgid the id of the message
 */
static void
xmms_courier_remove_pending (xmms_courier_t *courier, gint32 msgid)
{
	xmms_courier_pending_pool_t *pending = courier->pending_pool;

	g_mutex_lock (&pending->lock);
	g_tree_remove (pending->messages, GINT_TO_POINTER (msgid));
	g_mutex_unlock (&pending->lock);

	XMMS_DBG ("Removed pending message %d", msgid);
	return;
}

/**
 * Write a reply message to a client.
 *
 * @param dest the id of the client
 * @param c2c_msg the reply client-to-client message
 * @param cookie the cookie associated with the reply in the client
 * @param err for error reporting
 *
 * \sa xmms_ipc_send_message
 */
static void
write_reply (gint32 dest, xmmsv_t *c2c_msg, uint32_t cookie,
             xmms_error_t *err)
{
	xmms_ipc_msg_t *ipc_msg;

	ipc_msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_COURIER,
	                            XMMS_IPC_COMMAND_REPLY);

	xmms_ipc_msg_set_cookie (ipc_msg, cookie);
	xmms_ipc_msg_put_value (ipc_msg, c2c_msg);

	xmms_ipc_send_message (dest, ipc_msg, err);
	return;
}

/**
 * Callback for client connect signal.
 * Add the new client's id to the list of clients.
 */
static void
client_connected_cb (xmms_object_t *unused, xmmsv_t *val, gpointer userdata)
{
	gint32 id;
	xmms_courier_t *courier;

	xmmsv_get_int32 (val, &id);
	courier = (xmms_courier_t *) userdata;

	g_mutex_lock (&courier->clients_lock);
	courier->clients = g_list_append (courier->clients, GINT_TO_POINTER (id));
	g_mutex_unlock (&courier->clients_lock);

	return;
}

/**
 * Callback for client disconnect signal.
 * Remove the disconnected client from the list of clients and
 * remove all messages to/from that client from the pool of pending
 * messages.
 */
static void
client_disconnected_cb (xmms_object_t *unused, xmmsv_t *val, gpointer userdata)
{
	gint32 id;
	guint remove_count;
	GList *to_remove, *node;
	xmms_courier_t *courier;

	xmmsv_get_int32 (val, &id);
	courier = (xmms_courier_t *) userdata;

	g_mutex_lock (&courier->clients_lock);
	courier->clients = g_list_remove (courier->clients, GINT_TO_POINTER (id));
	courier->ready = g_list_remove (courier->ready, GINT_TO_POINTER (id));
	g_mutex_unlock (&courier->clients_lock);

	/* Prepare a list with the disconnected client's id */
	to_remove = g_list_append (NULL, GINT_TO_POINTER (id));

	/* Fill it with the ids of pending messages that involved that client */
	g_tree_foreach (courier->pending_pool->messages,
	                client_disconnected_foreach,
	                &to_remove);

	/* And remove those messages.
	 * Last entry in the list is still the id, so ignore it.
	 */
	node = to_remove;
	remove_count = g_list_length (to_remove)-1;
	while (remove_count-- > 0) {
		xmms_courier_remove_pending (courier, GPOINTER_TO_INT (node->data));
		node = g_list_next (node);
	}

	g_assert (g_list_length (node) == 1);
	g_assert (node->data == GINT_TO_POINTER (id));

	g_list_free (to_remove);

	return;
}

static gboolean
client_disconnected_foreach (gpointer key, gpointer value, gpointer userdata)
{
	gint32 client;
	GList *last, **to_remove;
	xmmsv_t *err_reply;
	xmms_error_t err;
	xmms_courier_pending_msg_t *entry;

	to_remove = (GList **) userdata;
	entry = (xmms_courier_pending_msg_t *) value;

	/* Get the disconnected client's id, which is
	 * always the last element in the list.
	 */
	last = g_list_last (*to_remove);
	client = GPOINTER_TO_INT (last->data);

	if (entry->destination == client || entry->sender == client) {
		if (entry->destination == client) {
			/* Reply an error to pending messages whose
			 * destination is the disconnected client.
			 */
			err_reply = xmmsv_new_error ("destination client disconnected");
			write_reply (entry->sender, err_reply, entry->cookie, &err);
			xmmsv_unref (err_reply);
		}

		*to_remove = g_list_prepend (*to_remove, key);
	}

	return FALSE;
}

static gint
compare_int32 (gconstpointer a, gconstpointer b, gpointer unused)
{
	gint32 va = GPOINTER_TO_INT (a);
	gint32 vb = GPOINTER_TO_INT (b);

	if (va < vb) {
		return -1;
	} else if (va > vb) {
		return 1;
	} else {
		return 0;
	}
}
