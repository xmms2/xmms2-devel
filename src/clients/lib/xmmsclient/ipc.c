/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <xmmsclient/xmmsclient.h>

#include <xmmsc/xmmsc_ipc_transport.h>
#include <xmmsc/xmmsc_ipc_msg.h>

#include <xmmsclientpriv/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient_ipc.h>
#include <xmmsclientpriv/xmmsclient_util.h>
#include <xmmsclientpriv/xmmsclient_queue.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmmsc/xmmsc_util.h>
#include <xmmsc/xmmsc_stdint.h>
#include <xmmsc/xmmsc_sockets.h>


struct xmmsc_ipc_St {
	xmms_ipc_transport_t *transport;
	xmms_ipc_msg_t *read_msg;
	x_list_t *results_list;
	x_queue_t *out_msg;
	char *error;
	bool disconnect;
	void *lockdata;
	void (*lockfunc)(void *lock);
	void (*unlockfunc)(void *lock);

	void (*disconnect_callback) (void *data);
	void *disconnect_data;
	void (*disconnect_data_free_func) (void *data);

	void (*need_out_callback) (int need_out, void *data);
	void *need_out_data;
	void (*need_out_data_free_func) (void *data);
};

static inline void xmmsc_ipc_lock (xmmsc_ipc_t *ipc);
static inline void xmmsc_ipc_unlock (xmmsc_ipc_t *ipc);
static void xmmsc_ipc_exec_msg (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg);


int
xmmsc_ipc_io_in_callback (xmmsc_ipc_t *ipc)
{
	bool disco = false;

	x_return_val_if_fail (ipc, false);
	x_return_val_if_fail (!ipc->disconnect, false);

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

	return !disco;
}

int
xmmsc_ipc_io_out (xmmsc_ipc_t *ipc)
{
	x_return_val_if_fail (ipc, false);

	return !x_queue_is_empty (ipc->out_msg) && !ipc->disconnect;
}

int
xmmsc_ipc_io_out_callback (xmmsc_ipc_t *ipc)
{
	bool disco = false;

	x_return_val_if_fail (ipc, false);
	x_return_val_if_fail (!ipc->disconnect, false);

	while (!x_queue_is_empty (ipc->out_msg)) {
		xmms_ipc_msg_t *msg = x_queue_peek_head (ipc->out_msg);
		if (xmms_ipc_msg_write_transport (msg, ipc->transport, &disco)) {
			x_queue_pop_head (ipc->out_msg);
			xmms_ipc_msg_destroy (msg);
		} else {
			break;
		}
	}

	if (disco) {
		xmmsc_ipc_disconnect (ipc);
	} else {
		if (ipc->need_out_callback)
			ipc->need_out_callback (xmmsc_ipc_io_out (ipc),
			                        ipc->need_out_data);
	}

	return !disco;
}

xmms_socket_t
xmmsc_ipc_fd_get (xmmsc_ipc_t *ipc)
{
	x_return_val_if_fail (ipc, -1);
	return xmms_ipc_transport_fd_get (ipc->transport);
}


const char *
xmmsc_ipc_error_get (xmmsc_ipc_t *ipc)
{
	x_return_val_if_fail (ipc, NULL);
	return ipc->error;
}

void
xmmsc_ipc_disconnect (xmmsc_ipc_t *ipc)
{
	ipc->disconnect = true;
	if (ipc->read_msg) {
		xmms_ipc_msg_destroy (ipc->read_msg);
		ipc->read_msg = NULL;
	}
	xmmsc_ipc_error_set (ipc, strdup ("Disconnected"));
	if (ipc->disconnect_callback) {
		ipc->disconnect_callback (ipc->disconnect_data);
	}
}

bool
xmmsc_ipc_disconnected (xmmsc_ipc_t *ipc)
{
	x_return_val_if_fail (ipc, true);
	return ipc->disconnect;
}

xmmsc_ipc_t *
xmmsc_ipc_init (void)
{
	xmmsc_ipc_t *ipc;
	ipc = x_new0 (xmmsc_ipc_t, 1);
	ipc->disconnect = false;
	ipc->results_list = NULL;
	ipc->out_msg = x_queue_new ();

	return ipc;
}

void
xmmsc_ipc_disconnect_set (xmmsc_ipc_t *ipc, void (*disconnect_callback) (void *), void *userdata, xmmsc_user_data_free_func_t free_func)
{
	x_return_if_fail (ipc);
	ipc->disconnect_callback = disconnect_callback;
	ipc->disconnect_data = userdata;
	ipc->disconnect_data_free_func = free_func;
}

void
xmmsc_ipc_need_out_callback_set (xmmsc_ipc_t *ipc, void (*callback) (int, void *), void *userdata, xmmsc_user_data_free_func_t free_func)
{
	x_return_if_fail (ipc);
	ipc->need_out_callback = callback;
	ipc->need_out_data = userdata;
	ipc->need_out_data_free_func = free_func;
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
	x_return_if_fail (ipc);
	x_return_if_fail (res);

	xmmsc_ipc_lock (ipc);
	ipc->results_list = x_list_prepend (ipc->results_list, res);
	xmmsc_ipc_unlock (ipc);
}

xmmsc_result_t *
xmmsc_ipc_result_lookup (xmmsc_ipc_t *ipc, uint32_t cookie)
{
	xmmsc_result_t *res = NULL;
	x_list_t *n;

	x_return_val_if_fail (ipc, NULL);

	xmmsc_ipc_lock (ipc);

	for (n = ipc->results_list; n; n = x_list_next (n)) {
		xmmsc_result_t *tmp = n->data;

		if (cookie == xmmsc_result_cookie_get (tmp)) {
			res = tmp;
			break;
		}
	}

	xmmsc_ipc_unlock (ipc);

	return res;
}

void
xmmsc_ipc_result_unregister (xmmsc_ipc_t *ipc, xmmsc_result_t *res)
{
	x_list_t *n;

	x_return_if_fail (ipc);
	x_return_if_fail (res);

	xmmsc_ipc_lock (ipc);

	for (n = ipc->results_list; n; n = x_list_next (n)) {
		xmmsc_result_t *tmp = n->data;

		if (xmmsc_result_cookie_get (res) == xmmsc_result_cookie_get (tmp)) {
			ipc->results_list = x_list_delete_link (ipc->results_list, n);
			xmmsc_result_clear_weakrefs (res);
			break;
		}
	}

	xmmsc_ipc_unlock (ipc);
}

void
xmmsc_ipc_error_set (xmmsc_ipc_t *ipc, char *error)
{
	x_return_if_fail (ipc);
	ipc->error = error;
}

void
xmmsc_ipc_wait_for_event (xmmsc_ipc_t *ipc, unsigned int timeout)
{
	fd_set rfdset;
	fd_set wfdset;
	struct timeval tmout;
	xmms_socket_t fd;

	x_return_if_fail (ipc);
	x_return_if_fail (!ipc->disconnect);

	tmout.tv_sec = timeout;
	tmout.tv_usec = 0;

	fd = xmms_ipc_transport_fd_get (ipc->transport);

	FD_ZERO (&rfdset);
	FD_SET (fd, &rfdset);

	FD_ZERO (&wfdset);
	if (xmmsc_ipc_io_out (ipc)) {
		FD_SET (fd, &wfdset);
	}

	if (select (fd + 1, &rfdset, &wfdset, NULL, &tmout) == SOCKET_ERROR) {
		return;
	}

	if (FD_ISSET (fd, &rfdset)) {
		if (!xmmsc_ipc_io_in_callback (ipc)) {
			return;
		}
	}
	if (FD_ISSET (fd, &wfdset)) {
		xmmsc_ipc_io_out_callback (ipc);
	}
}

bool
xmmsc_ipc_msg_write (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg, uint32_t cookie)
{
	x_return_val_if_fail (ipc, false);

	xmms_ipc_msg_set_cookie (msg, cookie);
	x_queue_push_tail (ipc->out_msg, msg);

	if (ipc->need_out_callback) {
		ipc->need_out_callback (1, ipc->need_out_data);
	}

	return true;
}


void
xmmsc_ipc_destroy (xmmsc_ipc_t *ipc)
{
	x_list_t *n;

	if (!ipc)
		return;

	for (n = ipc->results_list; n; n = x_list_next (n)) {
		xmmsc_result_t *tmp = n->data;
		xmmsc_result_clear_weakrefs (tmp);
	}

	x_list_free (ipc->results_list);
	if (ipc->transport) {
		xmms_ipc_transport_destroy (ipc->transport);
	}

	if (ipc->out_msg) {
		x_queue_free (ipc->out_msg);
	}

	if (ipc->read_msg) {
		xmms_ipc_msg_destroy (ipc->read_msg);
	}

	if (ipc->error) {
		free (ipc->error);
	}

	if (ipc->disconnect_data && ipc->disconnect_data_free_func) {
		xmmsc_user_data_free_func_t f = ipc->disconnect_data_free_func;
		f (ipc->disconnect_data);
	}

	if (ipc->need_out_data && ipc->need_out_data_free_func) {
		xmmsc_user_data_free_func_t f = ipc->need_out_data_free_func;
		f (ipc->need_out_data);
	}

	free (ipc);
}

bool
xmmsc_ipc_connect (xmmsc_ipc_t *ipc, char *path)
{
	x_return_val_if_fail (ipc, false);
	x_return_val_if_fail (path, false);

	ipc->transport = xmms_ipc_client_init (path);
	if (!ipc->transport) {
		ipc->error = strdup ("Could not init client!");
		return false;
	}
	return true;
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
		ipc->unlockfunc (ipc->lockdata);
}

static void
xmmsc_ipc_exec_msg (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg)
{
	xmmsc_result_t *res;

	res = xmmsc_ipc_result_lookup (ipc, xmms_ipc_msg_get_cookie (msg));

	if (!res) {
		xmms_ipc_msg_destroy (msg);
		return;
	}

	xmmsc_result_run (res, msg);
}
