/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include <sys/types.h>
#include <sys/select.h>
#include <string.h>

#include "xmms/util.h"
#include "xmms/ringbuf.h"
#include "xmms/signal_xmms.h"
#include "xmms/xmmsclient.h"

#include "xmms/ipc.h"
#include "xmms/ipc_transport.h"
#include "xmms/ipc_msg.h"

#include "internal/client_ipc.h"

typedef struct xmmsc_msg_queue_St {
	GQueue *queue;					/* complete messages */
	xmms_ipc_msg_t *incomplete;		/* message we have the header for but not enough data */
	guint8 buffer[16];				/* incomplete header */
	guint buffer_len;				/* bytes used in buffer */
	guint bytes_used;				/* bytes used in incomplete message */
} xmmsc_msg_queue_t;

struct xmmsc_ipc_St {
	xmms_ringbuf_t *read_buffer;
	xmms_ipc_transport_t *transport;
	gchar *error;
	gpointer private_data;
	gboolean disconnect;
	gint pollopts;
	GHashTable *results_table;
	xmmsc_msg_queue_t *queue;
	void *lockdata;
	void (*lockfunc)(void *lock);
	void (*unlockfunc)(void *lock);
	void (*disconnect_callback) (void *ipc);
	void *disconnect_data;
};

static inline void xmmsc_ipc_lock (xmmsc_ipc_t *ipc);
static inline void xmmsc_ipc_unlock (xmmsc_ipc_t *ipc);

xmmsc_msg_queue_t* xmmsc_msg_queue_new ();
xmms_ipc_msg_t* xmmsc_msg_queue_pop (xmmsc_msg_queue_t *queue);
void xmmsc_msg_queue_destroy (xmmsc_msg_queue_t *queue);
void xmmsc_msg_queue_write (xmmsc_msg_queue_t *queue, guint8 *buffer, guint size);

xmmsc_ipc_t *
xmmsc_ipc_init (void)
{
	xmmsc_ipc_t *ipc;
	ipc = g_new0 (xmmsc_ipc_t, 1);
	ipc->read_buffer = xmms_ringbuf_new_unlocked (XMMS_IPC_MSG_MAX_SIZE);
	ipc->disconnect = FALSE;
	ipc->pollopts = XMMSC_IPC_IO_IN;
	ipc->results_table = g_hash_table_new (NULL, NULL);
	ipc->queue = xmmsc_msg_queue_new ();

	return ipc;
}

void
xmmsc_ipc_disconnect_set (xmmsc_ipc_t *ipc, void (*disconnect_callback) (void *), void *userdata)
{
	x_return_if_fail (ipc);
	ipc->disconnect_callback = disconnect_callback;
	ipc->disconnect_data = userdata;
}

void
xmmsc_ipc_lock_set (xmmsc_ipc_t *ipc, void *lock, void (*lockfunc)(void *), void (*unlockfunc)(void *))
{
	ipc->lockdata = lock;
	ipc->lockfunc = lockfunc;
	ipc->unlockfunc = unlockfunc;
}

void
xmmsc_ipc_result_register (xmmsc_ipc_t *ipc, xmmsc_result_t *res)
{
	g_return_if_fail (ipc);
	g_return_if_fail (res);

	xmmsc_ipc_lock (ipc);
	g_hash_table_insert (ipc->results_table, GUINT_TO_POINTER (xmmsc_result_cid (res)), res);
	xmmsc_ipc_unlock (ipc);
}

xmmsc_result_t *
xmmsc_ipc_result_lookup (xmmsc_ipc_t *ipc, guint cid)
{
	xmmsc_result_t *res;
	g_return_val_if_fail (ipc, NULL);

	xmmsc_ipc_lock (ipc);
	res = g_hash_table_lookup (ipc->results_table, GUINT_TO_POINTER (cid));
	xmmsc_ipc_unlock (ipc);
	return res;
}

void
xmmsc_ipc_result_unregister (xmmsc_ipc_t *ipc, xmmsc_result_t *res)
{
	g_return_if_fail (ipc);
	g_return_if_fail (res);

	xmmsc_ipc_lock (ipc);
	g_hash_table_remove (ipc->results_table, GUINT_TO_POINTER (xmmsc_result_cid (res)));
	xmmsc_ipc_unlock (ipc);
}

static void
xmmsc_ipc_exec_msg (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg)
{
	xmmsc_result_t *res;

	res = xmmsc_ipc_result_lookup (ipc, msg->cid);

	if (!res) {
		xmms_ipc_msg_destroy (msg);
		return;
	}

	if (msg->cmd == XMMS_IPC_CMD_ERROR) {
		gchar *errstr;
		gint len;

		if (!xmms_ipc_msg_get_string_alloc (msg, &errstr, &len))
			errstr = g_strdup ("No errormsg!");

		xmmsc_result_seterror (res, errstr);
	}

	xmmsc_result_run (res, msg);
}

gboolean
xmmsc_ipc_io_in_callback (xmmsc_ipc_t *ipc)
{
	gchar buffer[4096];
	xmms_ipc_msg_t *msg = NULL;
	gint ret;
	g_return_val_if_fail (ipc, FALSE);

	do {
		ret = xmms_ipc_transport_read (ipc->transport, buffer, 4096);
		if (ret == -1) {
			break;
		} else if (ret == 0) {
			xmmsc_ipc_disconnect (ipc);
			break;
		}
		xmmsc_ipc_lock (ipc);
		xmmsc_msg_queue_write (ipc->queue, buffer, ret);
		xmmsc_ipc_unlock (ipc);
	} while (ret > 0);

#if HEAVY_DEBUG
	printf ("got %d bytes in ringbuffer!\n", xmms_ringbuf_bytes_used (ipc->read_buffer));
#endif

	xmmsc_ipc_lock (ipc);
	do {
		msg = xmmsc_msg_queue_pop (ipc->queue);
		if (!msg)
			continue;
		xmmsc_ipc_unlock (ipc);
#if HEAVY_DEBUG
		printf ("Read msg with command %d\n", msg->cmd);
#endif
		xmmsc_ipc_exec_msg (ipc, msg);
		xmmsc_ipc_lock (ipc);
	} while (msg);
	xmmsc_ipc_unlock (ipc);

	return TRUE;
}

gint
xmmsc_ipc_fd_get (xmmsc_ipc_t *ipc)
{
	g_return_val_if_fail (ipc, -1);
	return xmms_ipc_transport_fd_get (ipc->transport);
}

void
xmmsc_ipc_error_set (xmmsc_ipc_t *ipc, gchar *error)
{
	g_return_if_fail (ipc);
	ipc->error = error;
}

void
xmmsc_ipc_wait_for_event (xmmsc_ipc_t *ipc, guint timeout)
{
	fd_set fdset;
	struct timeval tmout;

	g_return_if_fail (ipc);
	tmout.tv_sec = timeout;
	tmout.tv_usec = 0;

	FD_ZERO (&fdset);
	FD_SET (xmms_ipc_transport_fd_get (ipc->transport), &fdset);

#ifdef HEAVY_DEBUG
	fprintf (stderr, "Waiting for event!\n");
#endif
	if (select (xmms_ipc_transport_fd_get (ipc->transport) + 1, &fdset, 
		    NULL, NULL, &tmout) == -1) {
#ifdef HEAVY_DEBUG
		fprintf (stderr, "select returned -1\n");
#endif
		return;
	}

	xmmsc_ipc_io_in_callback (ipc);
}

gboolean
xmmsc_ipc_msg_write (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg, guint32 cid)
{
	fd_set fdset;
	struct timeval tmout;

	g_return_val_if_fail (ipc, FALSE);
	g_return_val_if_fail (ipc, FALSE);

	tmout.tv_sec = 5;
	tmout.tv_usec = 0;

	FD_ZERO (&fdset);
	FD_SET (xmms_ipc_transport_fd_get (ipc->transport), &fdset);

	/* Block for 5 seconds ... */
	if (select (xmms_ipc_transport_fd_get (ipc->transport) +1, 
		    NULL, &fdset, NULL, &tmout) == -1) {
		return FALSE;
	}

	return xmms_ipc_msg_write_fd (xmms_ipc_transport_fd_get (ipc->transport), 
				      msg, cid);
}

void
xmmsc_ipc_disconnect (xmmsc_ipc_t *ipc)
{
	ipc->disconnect = TRUE;
	if (ipc->disconnect_callback) {
		ipc->disconnect_callback (ipc->disconnect_data);
	}
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
	if (!ipc)
		return;

	xmmsc_msg_queue_destroy (ipc->queue);
	xmms_ringbuf_destroy (ipc->read_buffer);
	g_hash_table_destroy (ipc->results_table);
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
	g_return_val_if_fail (ipc, FALSE);
	g_return_val_if_fail (path, FALSE);

	ipc->transport = xmms_ipc_client_init (path);
	if (!ipc->transport) {
		ipc->error = "Could not init client!";
		return FALSE;
	}

	return TRUE;
}

static inline void
xmmsc_ipc_lock (xmmsc_ipc_t *ipc)
{
	if (ipc->lockdata)
		ipc->lockfunc (ipc->lockdata);
}

static inline void
xmmsc_ipc_unlock (xmmsc_ipc_t *ipc)
{
	if (ipc->lockdata)
		ipc->lockfunc (ipc->lockdata);
}

xmmsc_msg_queue_t *
xmmsc_msg_queue_new ()
{
	xmmsc_msg_queue_t *queue = g_new0 (xmmsc_msg_queue_t, 1);
	g_return_val_if_fail (queue, NULL);

	queue->queue = g_queue_new ();
	queue->incomplete = NULL;
	queue->buffer_len = 0;
	queue->bytes_used = 0;

	return queue;
}

void
xmmsc_msg_queue_destroy (xmmsc_msg_queue_t *queue)
{
	g_return_if_fail (queue);

	while (g_queue_is_empty (queue->queue) != TRUE) {
		xmms_ipc_msg_t *msg = g_queue_pop_head (queue->queue);
		xmms_ipc_msg_destroy (msg);
	}

	if (queue->incomplete != NULL) {
		xmms_ipc_msg_destroy (queue->incomplete);
	}

	g_queue_free (queue->queue);
	g_free (queue);
}

xmms_ipc_msg_t *
xmmsc_msg_queue_pop (xmmsc_msg_queue_t *queue)
{
	g_return_val_if_fail (queue, NULL);

	return g_queue_pop_head (queue->queue);
}

void
xmmsc_msg_queue_write (xmmsc_msg_queue_t *queue, guint8 *buffer, guint size)
{
	g_return_if_fail (queue);
	g_return_if_fail (buffer);

	while (size > 0 || queue->buffer_len == 16) {
		if (queue->buffer_len > 0) {
			xmms_ipc_msg_t *msg;
			guint32 cmd, object, cmdid, length;
			guint towrite = MIN(size, 16 - queue->buffer_len);

			memcpy (queue->buffer + queue->buffer_len, buffer, towrite);
			queue->buffer_len += towrite;
			buffer += towrite;
			size -= towrite;

			if (queue->buffer_len != 16) {
				return;
			}

			memcpy (&object, queue->buffer, sizeof (object));
			memcpy (&cmd, queue->buffer + 4, sizeof (cmd));
			memcpy (&cmdid, queue->buffer + 8, sizeof (cmdid));
			memcpy (&length, queue->buffer + 12, sizeof (length));

			object = g_ntohl (object);
			cmd = g_ntohl (cmd);
			cmdid = g_ntohl (cmdid);
			length = g_ntohl (length);

			msg = xmms_ipc_msg_new (object, cmd);
			msg->cid = cmdid;
			msg->data_length = length;
			msg->data = g_realloc (msg->data, length);
			msg->size = length;

			queue->incomplete = msg;
			queue->bytes_used = 0;
			queue->buffer_len = 0;
		}

		if (queue->incomplete != NULL) {
			xmms_ipc_msg_t *msg = queue->incomplete;
			guint towrite = MIN(size, msg->data_length - queue->bytes_used);

			memcpy (msg->data + queue->bytes_used, buffer, towrite);
			queue->bytes_used += towrite;
			buffer += towrite;
			size -= towrite;

			if (queue->bytes_used == msg->data_length) {
				g_queue_push_tail (queue->queue, msg);
				queue->incomplete = NULL;
				queue->bytes_used = 0;
			}
		}

		if (size > 0) {
			guint towrite = MIN(size, 16 - queue->buffer_len);
			memcpy (queue->buffer + queue->buffer_len, buffer, towrite);
			queue->buffer_len += towrite;
			buffer += towrite;
			size -= towrite;
		}
	}
}

