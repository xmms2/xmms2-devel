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
#include "xmms/signal_xmms.h"
#include "xmms/xmmsclient.h"

#include "xmms/ipc.h"
#include "xmms/ipc_transport.h"
#include "xmms/ipc_msg.h"

#include "internal/client_ipc.h"

struct xmmsc_ipc_St {
	xmms_ipc_transport_t *transport;
	xmms_ipc_msg_t *read_msg;
	GQueue *out_msg;
	gchar *error;
	gboolean disconnect;
	GHashTable *results_table;
	void *lockdata;
	void (*lockfunc)(void *lock);
	void (*unlockfunc)(void *lock);
	void (*disconnect_callback) (void *ipc);
	void *disconnect_data;
};

static inline void xmmsc_ipc_lock (xmmsc_ipc_t *ipc);
static inline void xmmsc_ipc_unlock (xmmsc_ipc_t *ipc);

xmmsc_ipc_t *
xmmsc_ipc_init (void)
{
	xmmsc_ipc_t *ipc;
	ipc = g_new0 (xmmsc_ipc_t, 1);
	ipc->disconnect = FALSE;
	ipc->results_table = g_hash_table_new (NULL, NULL);
	ipc->out_msg = g_queue_new ();

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

	res = xmmsc_ipc_result_lookup (ipc, xmms_ipc_msg_get_cid (msg));

	if (!res) {
		xmms_ipc_msg_destroy (msg);
		return;
	}

	if (xmms_ipc_msg_get_cmd (msg) == XMMS_IPC_CMD_ERROR) {
		gchar *errstr;
		gint len;

		if (!xmms_ipc_msg_get_string_alloc (msg, &errstr, &len))
			errstr = g_strdup ("No errormsg!");

		xmmsc_result_seterror (res, errstr);
	}

	xmmsc_result_run (res, msg);
}

int
xmmsc_ipc_io_in_callback (xmmsc_ipc_t *ipc)
{
	gboolean disco = FALSE;

	g_return_val_if_fail (ipc, FALSE);
	g_return_val_if_fail (!ipc->disconnect, FALSE);

	while (!disco) {
		if (!ipc->read_msg)
			ipc->read_msg = xmms_ipc_msg_alloc ();
		
		if (xmms_ipc_msg_read_transport (ipc->read_msg, ipc->transport, &disco)) {
			xmms_ipc_msg_t *msg = ipc->read_msg;
			/* must unset read_msg here,
			   because exec_msg can cause reentrancy */
			ipc->read_msg = NULL;
			xmmsc_ipc_exec_msg (ipc, msg);
		} else {
			break;
		}
	}

	if (disco)
		xmmsc_ipc_disconnect (ipc);

	return TRUE;
}

int
xmmsc_ipc_io_out (xmmsc_ipc_t *ipc)
{
	g_return_val_if_fail (ipc, FALSE);

	return !g_queue_is_empty (ipc->out_msg) && !ipc->disconnect;
}

int
xmmsc_ipc_io_out_callback (xmmsc_ipc_t *ipc)
{
	gboolean disco = FALSE;

	g_return_val_if_fail (ipc, FALSE);
	g_return_val_if_fail (!ipc->disconnect, FALSE);

	while (!g_queue_is_empty (ipc->out_msg)) {
		xmms_ipc_msg_t *msg = g_queue_peek_head (ipc->out_msg);
		if (xmms_ipc_msg_write_transport (msg, ipc->transport, &disco)) {
			g_queue_pop_head (ipc->out_msg);
			xmms_ipc_msg_destroy (msg);
		} else {
			break;
		}
	}

	if (disco)
		xmmsc_ipc_disconnect (ipc);

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

const char *
xmmsc_ipc_error_get (xmmsc_ipc_t *ipc)
{
	g_return_val_if_fail (ipc, NULL);
	return ipc->error;
}

void
xmmsc_ipc_wait_for_event (xmmsc_ipc_t *ipc, guint timeout)
{
	fd_set rfdset;
	fd_set wfdset;
	struct timeval tmout;
	int fd;

	g_return_if_fail (ipc);
	g_return_if_fail (!ipc->disconnect);

	tmout.tv_sec = timeout;
	tmout.tv_usec = 0;

	fd = xmms_ipc_transport_fd_get (ipc->transport);

	FD_ZERO (&rfdset);
	FD_SET (fd, &rfdset);

	FD_ZERO (&wfdset);
	if (xmmsc_ipc_io_out (ipc))
		FD_SET (fd, &wfdset);

	if (select (fd + 1, &rfdset, &wfdset, NULL, &tmout) == -1) {
		return;
	}

	if (FD_ISSET(fd, &rfdset))
		xmmsc_ipc_io_in_callback (ipc);
	if (FD_ISSET(fd, &wfdset))
		xmmsc_ipc_io_out_callback (ipc);
}

gboolean
xmmsc_ipc_msg_write (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg, guint32 cid)
{
	g_return_val_if_fail (ipc, FALSE);
	xmms_ipc_msg_set_cid (msg, cid);
	g_queue_push_tail (ipc->out_msg, msg);
	return TRUE;
}

void
xmmsc_ipc_disconnect (xmmsc_ipc_t *ipc)
{
	ipc->disconnect = TRUE;
	if (ipc->read_msg) {
		xmms_ipc_msg_destroy (ipc->read_msg);
		ipc->read_msg = NULL;
	}
	xmmsc_ipc_error_set (ipc, g_strdup ("Disconnected"));
	if (ipc->disconnect_callback) {
		ipc->disconnect_callback (ipc->disconnect_data);
	}
}

void
xmmsc_ipc_destroy (xmmsc_ipc_t *ipc)
{
	if (!ipc)
		return;

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
		ipc->error = g_strdup ("Could not init client!");
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

