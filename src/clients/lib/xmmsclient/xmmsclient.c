/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <sys/types.h>

#include <xmmscpriv/xmms_list.h>

#include <xmmsclient/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmmsc/xmmsc_stdint.h>
#include <xmmsc/xmmsc_stringport.h>
#include <xmmsc/xmmsc_util.h>


#define XMMS_MAX_URI_LEN 1024

static void xmmsc_deinit (xmmsc_connection_t *c);

/*
 * Public methods
 */

/**
 * @mainpage
 * @image html pixmaps/xmms2-128.png
 */

/**
 * @defgroup XMMSClient
 * @brief This functions are used to connect a client software
 * to the XMMS2 daemon.
 *
 * For proper integration with a client you need to hook the XMMSIPC
 * to your clients mainloop. XMMS2 ships with a couple of default
 * mainloop integrations but can easily be extended to fit your own
 * application.
 *
 * There are three kinds of messages that will be involved in communication
 * with the XMMS2 server.
 * - Commands: Sent by the client to the server with arguments. Commands will
 * generate a reply to the client.
 * - Broadcasts: Sent by the server to the client if requested. Requesting a
 * broadcast is done by calling one of the xmmsc_broadcast functions.
 * - Signals: Like broadcasts but they are throttled, the client has to request
 * the next signal when the callback is called. #xmmsc_result_restart is used to
 * "restart" the next signal.
 *
 * Each client command will return a #xmmsc_result_t which holds the command id
 * that will be used to map the result back to the right caller. The #xmmsc_result_t
 * is used to get the result from the server.
 *
 * @{
 */

/**
 * Initializes a xmmsc_connection_t. Returns %NULL if you
 * runned out of memory.
 *
 * @return a xmmsc_connection_t that should be unreferenced with
 * xmmsc_unref.
 *
 * @sa xmmsc_unref
 */

/* 14:47 <irlanders> isalnum(c) || c == '_' || c == '-'
 */

xmmsc_connection_t *
xmmsc_init (const char *clientname)
{
	xmmsc_connection_t *c;
	int i = 0;
	char j;

	x_api_error_if (!clientname, "with NULL clientname", NULL);

	if (!(c = x_new0 (xmmsc_connection_t, 1))) {
		return NULL;
	}

	while (clientname[i]) {
		j = clientname[i];
		if (!isalnum (j) && j != '_' && j != '-') {
			/* snyggt! */
			free (c);
			x_api_error_if (true, "clientname contains invalid chars, just alphanumeric chars are allowed!", NULL);
		}
		i++;
	}

	if (!(c->clientname = strdup (clientname))) {
		free (c);
		return NULL;
	}

	c->visc = 0;
	c->visv = NULL;

	c->sc_root = NULL;
	return xmmsc_ref (c);
}

static xmmsc_result_t *
xmmsc_send_hello (xmmsc_connection_t *c)
{
	const int protocol_version = XMMS_IPC_PROTOCOL_VERSION;

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_MAIN, XMMS_IPC_CMD_HELLO,
	                       XMMSV_LIST_ENTRY_INT (protocol_version),
	                       XMMSV_LIST_ENTRY_STR (c->clientname),
	                       XMMSV_LIST_END);
}

/**
 * Connects to the XMMS server.
 * If ipcpath is NULL, it will try to open the default path.
 *
 * @param c The connection to the server. This must be initialized
 * with #xmmsc_init first.
 * @param ipcpath The IPC path, it's broken down like this: &lt;protocol&gt;://&lt;path&gt;[:&lt;port&gt;].
 * If ipcpath is %NULL it will default to "unix:///tmp/xmms-ipc-<username>"
 * - Protocol could be "tcp" or "unix"
 * - Path is either the UNIX socket, or the ipnumber of the server.
 * - Port is only used when the protocol tcp.
 *
 * @returns TRUE on success and FALSE if some problem
 * occured. call xmmsc_get_last_error to find out.
 *
 * @sa xmmsc_get_last_error
 */

int
xmmsc_connect (xmmsc_connection_t *c, const char *ipcpath)
{
	xmmsc_ipc_t *ipc;
	xmmsc_result_t *result;
	xmmsv_t *value;

	const char *buf;

	x_api_error_if (!c, "with a NULL connection", false);

	if (!ipcpath) {
		if (!xmms_default_ipcpath_get (c->path, sizeof (c->path))) {
			return false;
		}
	} else {
		snprintf (c->path, sizeof (c->path), "%s", ipcpath);
	}

	ipc = xmmsc_ipc_init ();

	if (!xmmsc_ipc_connect (ipc, c->path)) {
		c->error = strdup ("xmms2d is not running.");
		xmmsc_ipc_destroy (ipc);
		return false;
	}

	c->ipc = ipc;
	result = xmmsc_send_hello (c);
	xmmsc_result_wait (result);
	value = xmmsc_result_get_value (result);
	if (xmmsv_is_error (value)) {
		xmmsv_get_error (value, &buf);
		c->error = strdup (buf);
		xmmsc_result_unref (result);
		return false;
	} else {
		xmmsv_get_int64 (value, &c->id);
	}
	xmmsc_result_unref (result);
	return true;
}

/**
 * Set the disconnect callback. It will be called when client will
 * be disconnected.
 */
void
xmmsc_disconnect_callback_set (xmmsc_connection_t *c,
                               xmmsc_disconnect_func_t callback,
                               void *userdata)
{
	xmmsc_disconnect_callback_set_full (c, callback, userdata, NULL);
}

void
xmmsc_disconnect_callback_set_full (xmmsc_connection_t *c,
                                    xmmsc_disconnect_func_t callback,
                                    void *userdata,
                                    xmmsc_user_data_free_func_t free_func)
{
	x_check_conn (c,);
	xmmsc_ipc_disconnect_set (c->ipc, callback, userdata, free_func);
}

/**
 * Returns a string that descibes the last error.
 */
char *
xmmsc_get_last_error (xmmsc_connection_t *c)
{
	x_api_error_if (!c, "with a NULL connection", false);
	return c->error;
}

/**
 * Dereference the #xmmsc_connection_t and free
 * the memory when reference count reaches zero.
 */
void
xmmsc_unref (xmmsc_connection_t *c)
{
	x_api_error_if (!c, "with a NULL connection",);
	x_api_error_if (c->ref < 1, "with a freed connection",);

	c->ref--;
	if (c->ref == 0) {
		xmmsc_deinit (c);
	}
}

/**
 * References the #xmmsc_connection_t
 *
 * @param c the connection to reference.
 * @return c
 */
xmmsc_connection_t *
xmmsc_ref (xmmsc_connection_t *c)
{
	x_api_error_if (!c, "with a NULL connection", NULL);

	c->ref++;

	return c;
}

/**
 * @internal
 * Frees up any resources used by xmmsc_connection_t
 */
static void
xmmsc_deinit (xmmsc_connection_t *c)
{
	xmmsc_ipc_destroy (c->ipc);

	if (c->sc_root) {
		xmmsc_sc_interface_entity_destroy (c->sc_root);
	}

	free (c->error);
	free (c->clientname);
	free (c);
}

/**
 * Set locking functions for a connection. Allows simultanous usage of
 * a connection from several threads.
 *
 * @param c connection
 * @param lock the locking primitive passed to the lock and unlock functions
 * @param lockfunc function called when entering critical region, called with lock as argument.
 * @param unlockfunc funciotn called when leaving critical region.
 */
void
xmmsc_lock_set (xmmsc_connection_t *c, void *lock, void (*lockfunc)(void *), void (*unlockfunc)(void *))
{
	x_check_conn (c,);

	xmmsc_ipc_lock_set (c->ipc, lock, lockfunc, unlockfunc);
}

/**
 * Tell the server to quit. This will terminate the server.
 * If you only want to disconnect, use #xmmsc_unref()
 */
xmmsc_result_t *
xmmsc_quit (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_MAIN, XMMS_IPC_CMD_QUIT);
}

/**
 * Request the quit broadcast.
 * Will be called when the server is terminating.
 */
xmmsc_result_t *
xmmsc_broadcast_quit (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_QUIT);
}

/**
 * Get the absolute path to the user config dir.
 *
 * @param buf A char buffer
 * @param len The length of buf (XMMS_PATH_MAX is a good choice)
 * @return A pointer to buf, or NULL if an error occurred.
 */
const char *
xmmsc_userconfdir_get (char *buf, int len)
{
	return xmms_userconfdir_get (buf, len);
}


/** @} */

/**
 * @internal
 */

static uint32_t
xmmsc_next_id (xmmsc_connection_t *c)
{
	return c->cookie++;
}

static uint32_t
xmmsc_write_msg_to_ipc (xmmsc_connection_t *c, xmms_ipc_msg_t *msg)
{
	uint32_t cookie;

	cookie = xmmsc_next_id (c);

	xmmsc_ipc_msg_write (c->ipc, msg, cookie);

	return cookie;
}

xmmsc_result_t *
xmmsc_send_broadcast_msg (xmmsc_connection_t *c, int signalid)
{
	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_BROADCAST,
	                       XMMSV_LIST_ENTRY_INT (signalid),
	                       XMMSV_LIST_END);
}


uint32_t
xmmsc_write_signal_msg (xmmsc_connection_t *c, int signalid)
{
	xmms_ipc_msg_t *msg;
	xmmsv_t *args;
	uint32_t cookie;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);

	args = xmmsv_build_list (XMMSV_LIST_ENTRY_INT (signalid),
	                         XMMSV_LIST_END);

	xmms_ipc_msg_put_value (msg, args);
	xmmsv_unref (args);

	cookie = xmmsc_write_msg_to_ipc (c, msg);

	return cookie;
}

xmmsc_result_t *
xmmsc_send_signal_msg (xmmsc_connection_t *c, int signalid)
{
	xmmsc_result_t *res;

	res = xmmsc_send_cmd (c, XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL,
	                      XMMSV_LIST_ENTRY_INT (signalid), XMMSV_LIST_END);

	xmmsc_result_restartable (res, signalid);

	return res;
}

xmmsc_result_t *
xmmsc_send_msg_no_arg (xmmsc_connection_t *c, int object, int method)
{
	uint32_t cookie;
	xmms_ipc_msg_t *msg;
	xmmsv_t *args;

	msg = xmms_ipc_msg_new (object, method);

	args = xmmsv_new_list ();
	xmms_ipc_msg_put_value (msg, args);
	xmmsv_unref (args);

	cookie = xmmsc_write_msg_to_ipc (c, msg);

	return xmmsc_result_new (c, XMMSC_RESULT_CLASS_DEFAULT, cookie);
}

xmmsc_result_t *
xmmsc_send_msg (xmmsc_connection_t *c, xmms_ipc_msg_t *msg)
{
	uint32_t cookie;
	xmmsc_result_type_t type;

	cookie = xmmsc_write_msg_to_ipc (c, msg);

	type = XMMSC_RESULT_CLASS_DEFAULT;
	if (xmms_ipc_msg_get_object (msg) == XMMS_IPC_OBJECT_SIGNAL) {
		if (xmms_ipc_msg_get_cmd (msg) == XMMS_IPC_CMD_SIGNAL) {
			type = XMMSC_RESULT_CLASS_SIGNAL;
		} else if (xmms_ipc_msg_get_cmd (msg) == XMMS_IPC_CMD_BROADCAST) {
			type = XMMSC_RESULT_CLASS_BROADCAST;
		}
	}

	return xmmsc_result_new (c, type, cookie);
}

xmmsc_result_t *
xmmsc_send_cmd (xmmsc_connection_t *c, int obj, int cmd, ...)
{
	xmmsv_t *first_arg;
	xmms_ipc_msg_t *msg;
	xmmsv_t *args;
	va_list ap;

	msg = xmms_ipc_msg_new (obj, cmd);

	va_start (ap, cmd);
	first_arg = va_arg (ap, xmmsv_t *);
	args = xmmsv_build_list_va (first_arg, ap);
	va_end (ap);

	xmms_ipc_msg_put_value (msg, args);
	xmmsv_unref (args);

	return xmmsc_send_msg (c, msg);
}

uint32_t
xmmsc_send_cmd_cookie (xmmsc_connection_t *c, int obj, int cmd, ...)
{
	xmmsv_t *first_arg;
	xmms_ipc_msg_t *msg;
	xmmsv_t *args;
	va_list ap;

	msg = xmms_ipc_msg_new (obj, cmd);

	va_start (ap, cmd);
	first_arg = va_arg (ap, xmmsv_t *);
	args = xmmsv_build_list_va (first_arg, ap);
	va_end (ap);

	xmms_ipc_msg_put_value (msg, args);
	xmmsv_unref (args);

	return xmmsc_write_msg_to_ipc (c, msg);
}

/**
 * @defgroup IOFunctions IOFunctions
 * @ingroup XMMSClient
 *
 * @brief Functions for integrating the xmms client library with an
 * existing mainloop. Only to be used if there isn't already
 * integration written (glib, corefoundation or ecore).
 *
 * @{
 */

/**
 * Check for pending output.
 *
 * Used to determine if there is any outgoing data enqueued and the
 * mainloop should flag this socket for writing.
 *
 * @param c connection to check
 * @return 1 output is requested, 0 otherwise
 *
 * @sa xmmsc_io_need_out_callback_set
 */
int
xmmsc_io_want_out (xmmsc_connection_t *c)
{
	x_check_conn (c, -1);

	return xmmsc_ipc_io_out (c->ipc);
}

/**
 * Write pending data.
 *
 * Should be called when the mainloop flags that writing is available
 * on the socket.
 *
 * @returns 1 if everything is well, 0 if the connection is broken.
 */
int
xmmsc_io_out_handle (xmmsc_connection_t *c)
{
	x_check_conn (c, -1);
	x_api_error_if (!xmmsc_ipc_io_out (c->ipc), "without pending output", -1);

	return xmmsc_ipc_io_out_callback (c->ipc);
}

/**
 * Read available data
 *
 * Should be called when the mainloop flags that reading is available
 * on the socket.
 *
 * @returns 1 if everything is well, 0 if the connection is broken.
 */
int
xmmsc_io_in_handle (xmmsc_connection_t *c)
{
	x_check_conn (c, -1);
	x_api_error_if (xmmsc_ipc_disconnected (c->ipc), "although the xmms2 daemon is not connected", -1);

	return xmmsc_ipc_io_in_callback (c->ipc);
}

/**
 * Retrieve filedescriptor for connection.
 *
 * Gets the underlaying filedescriptor for this connection, or -1 upon
 * error. To be used in a mainloop to do poll/select on. Reading
 * writing should *NOT* be done on this fd, #xmmsc_io_in_handle and
 * #xmmsc_io_out_handle MUST be used to handle reading and writing.
 *
 * @returns underlaying filedescriptor, or -1 on error
 */
int
xmmsc_io_fd_get (xmmsc_connection_t *c)
{
	x_check_conn (c, -1);
	return xmmsc_ipc_fd_get (c->ipc);
}

/**
 * Set callback for enabling/disabling writing.
 *
 * If the mainloop doesn't provide a mechanism to run code before each
 * iteration this function allows registration of a callback to be
 * called when output is needed or not needed any more. The arguments
 * to the callback are flag and userdata; flag is 1 if output is
 * wanted, 0 if not.
 *
 */
void
xmmsc_io_need_out_callback_set (xmmsc_connection_t *c, void (*callback) (int, void*), void *userdata)
{
	return xmmsc_io_need_out_callback_set_full (c, callback, userdata, NULL);
}

void
xmmsc_io_need_out_callback_set_full (xmmsc_connection_t *c, void (*callback) (int, void*), void *userdata, xmmsc_user_data_free_func_t free_func)
{
	x_check_conn (c,);
	xmmsc_ipc_need_out_callback_set (c->ipc, callback, userdata, free_func);
}

/**
 * Flag connection as disconnected.
 *
 * To be called when the mainloop signals disconnection of the
 * connection. This is optional, any call to #xmmsc_io_out_handle or
 * #xmmsc_io_in_handle will notice the disconnection and handle it
 * accordingly.
 *
 */
void
xmmsc_io_disconnect (xmmsc_connection_t *c)
{
	x_check_conn (c,);

	xmmsc_ipc_disconnect (c->ipc);
}

/**
 * @}
 */
