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

/** @file 
 * XMMS client lib.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>

#include <pwd.h>
#include <sys/types.h>

#include "xmmsclient/xmmsclient_hash.h"
#include "xmmsclient/xmmsclient_list.h"

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsc/xmmsc_idnumbers.h"

#define XMMS_MAX_URI_LEN 1024

#define _REGULARCHAR(a) ((a>=65 && a<=90) || (a>=97 && a<=122)) || (isdigit (a))

static void xmmsc_deinit (xmmsc_connection_t *c);

static uint32_t cmd_id;

/*
 * Public methods
 */

/**
 * @defgroup XMMSClient XMMSClient
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

xmmsc_connection_t *
xmmsc_init (char *clientname)
{
	xmmsc_connection_t *c;

	x_return_val_if_fail (clientname, NULL);

	if (!(c = x_new0 (xmmsc_connection_t, 1))) {
		return NULL;
	}

	xmmsc_ref (c);
	c->clientname = strdup (clientname);
	cmd_id = 0;

	return c;
}

static xmmsc_result_t *
xmmsc_send_hello (xmmsc_connection_t *c)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *result;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MAIN, XMMS_IPC_CMD_HELLO);
	xmms_ipc_msg_put_int32 (msg, 1); /* PROTOCOL VERSION */
	xmms_ipc_msg_put_string (msg, c->clientname);

	result = xmmsc_send_msg (c, msg);

	return result;
}

/**
 * Connects to the XMMS server.
 * If ipcpath is NULL, it will try to open the default path.
 * 
 * @param c The connection to the server. This must be initialized
 * with #xmmsc_init first.
 * @param ipcpath The IPC path, it's broken down like this: <protocol>://<path>[:<port>].
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
	int i, ret;

	char path[256];

	if (!c)
		return FALSE;

	if (!ipcpath) {
		struct passwd *pwd;

		pwd = getpwuid (getuid ());
		if (!pwd || !pwd->pw_name)
			return FALSE;

		snprintf (path, 256, "unix:///tmp/xmms-ipc-%s", pwd->pw_name);
	} else {
		snprintf (path, 256, "%s", ipcpath);
	}

	ipc = xmmsc_ipc_init ();
	
	if (!xmmsc_ipc_connect (ipc, path)) {
		c->error = "xmms2d is not running.";
		return FALSE;
	}

	c->ipc = ipc;

	result = xmmsc_send_hello (c);
	xmmsc_result_wait (result);
	ret = xmmsc_result_get_uint (result, &i);
	xmmsc_result_unref (result);

	return ret;
}

/**
 * Set the disconnect callback. It will be called when client will
 * be disconnected.
 */
void
xmmsc_disconnect_callback_set (xmmsc_connection_t *c, void (*callback) (void*), void *userdata)
{
	xmmsc_ipc_disconnect_set (c->ipc, callback, userdata);
}

/**
 * Returns a string that descibes the last error.
 */
char *
xmmsc_get_last_error (xmmsc_connection_t *c)
{
	return c->error;
}

/**
 * Dereference the #xmmsc_connection_t and free
 * the memory when reference count reaches zero.
 */
void
xmmsc_unref (xmmsc_connection_t *c)
{
	x_return_if_fail (c);

	c->ref--;
	if (c->ref == 0) {
		xmmsc_deinit (c);
	}
}

/**
 * @internal
 */
void
xmmsc_ref (xmmsc_connection_t *c)
{
	x_return_if_fail (c);

	c->ref++;
}

/**
 * @internal
 * Frees up any resources used by xmmsc_connection_t
 */
static void
xmmsc_deinit (xmmsc_connection_t *c)
{
	xmmsc_ipc_destroy (c->ipc);

	free (c->clientname);
	free(c);
}

/**
 * Set locking functions for a connection. Allows simultanous usage of
 * a connection from several threads.
 *
 * @param conn connection
 * @param lock the locking primitive passed to the lock and unlock functions
 * @param lockfunc function called when entering critical region, called with lock as argument.
 * @param unlockfunc funciotn called when leaving critical region.
 */
void
xmmsc_lock_set (xmmsc_connection_t *conn, void *lock, void (*lockfunc)(void *), void (*unlockfunc)(void *))
{
	xmmsc_ipc_lock_set (conn->ipc, lock, lockfunc, unlockfunc);
}

/**
 * Tell the server to quit. This will terminate the server.
 * If you only want to disconnect, use #xmmsc_unref()
 */

xmmsc_result_t *
xmmsc_quit (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_MAIN, XMMS_IPC_CMD_QUIT);
}

/**
 * This function will make a pretty string about the information in
 * the mediainfo hash supplied to it.
 * @param target A allocated char *
 * @param len Length of target
 * @param fmt A format string to use. You can insert items from the hash by
 * using specialformat "${field}".
 * @param table The x_hash_t that you got from xmmsc_result_get_mediainfo
 * @returns The number of chars written to #target
 */

int
xmmsc_entry_format (char *target, int len, const char *fmt, x_hash_t *table)
{
	const char *pos;

	if (!target) {
		return 0;
	}

	if (!fmt) {
		return 0;
	}

	memset (target, '\0', len);

	pos = fmt;
	while (strlen (target) + 1 < len) {
		char *next_key, *key, *result, *end;
		int keylen;

		next_key = strstr (pos, "${");
		if (!next_key) {
			strncat (target, pos, len - strlen (target) - 1);
			break;
		}

		strncat (target, pos, MIN (next_key - pos, len - strlen (target) - 1));
		keylen = strcspn (next_key + 2, "}");
		key = malloc (keylen + 1);

		if (!key) {
			fprintf (stderr, "Unable to allocate %u bytes of memory, OOM?", keylen);
			break;
		}

		memset (key, '\0', keylen + 1);
		strncpy (key, next_key + 2, keylen);

		if (strcmp (key, "seconds") == 0) {
			char seconds[10];
			int d;
			char *duration = x_hash_lookup (table, "duration");

			if (!duration) {
				strncat (target, "00", len - strlen (target) - 1);
				goto cont;
			}

			d = atoi (duration);
			snprintf (seconds, sizeof seconds, "%02d", (d/1000)%60);
			strncat (target, seconds, len - strlen (target) - 1);
			goto cont;
		}

		if (strcmp (key, "minutes") == 0) {
			char minutes[10];
			int d;
			char *duration = x_hash_lookup (table, "duration");

			if (!duration) {
				strncat (target, "00", len - strlen (target) - 1);
				goto cont;
			}

			d = atoi (duration);
			snprintf (minutes, sizeof minutes, "%02d", d/60000);
			strncat (target, minutes, len - strlen (target) - 1);
			goto cont;
		}

		result = x_hash_lookup (table, key);

		if (result) {
			strncat (target, result, len - strlen (target) - 1);
		}

cont:
		free (key);
		end = strchr (next_key, '}');

		if (!end) {
			break;
		}

		pos = end + 1;
	}

	return strlen (target);
}

/**
 * Disconnect from a broadcast. This will cause the server
 * to not send the broadcasts anymore.
 */
void
xmmsc_broadcast_disconnect (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	/** @todo tell the server that we're not interested in the signal
	 *        any more.
	 */
	xmmsc_result_unref (res);
}

/**
 * Disconnect from a signal. This will cause the server
 * to stop sending the signals.
 */
void
xmmsc_signal_disconnect (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	xmmsc_result_unref (res);
}

/** @} */

/**
 * @internal
 */

static uint32_t
xmmsc_next_id (void)
{
	return cmd_id++;
}

xmmsc_result_t *
xmmsc_send_broadcast_msg (xmmsc_connection_t *c, uint32_t signalid)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *res;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_BROADCAST);
	xmms_ipc_msg_put_uint32 (msg, signalid);
	
	res = xmmsc_send_msg (c, msg);

	xmmsc_result_restartable (res, signalid);

	return res;
}


xmmsc_result_t *
xmmsc_send_signal_msg (xmmsc_connection_t *c, uint32_t signalid)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *res;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);
	xmms_ipc_msg_put_uint32 (msg, signalid);
	
	res = xmmsc_send_msg (c, msg);
	
	xmmsc_result_restartable (res, signalid);

	return res;
}

xmmsc_result_t *
xmmsc_send_msg_no_arg (xmmsc_connection_t *c, int object, int method)
{
	uint32_t cid;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (object, method);

	cid = xmmsc_next_id ();
	xmmsc_ipc_msg_write (c->ipc, msg, cid);

	return xmmsc_result_new (c, cid);
}

xmmsc_result_t *
xmmsc_send_msg (xmmsc_connection_t *c, xmms_ipc_msg_t *msg)
{
	uint32_t cid;
	cid = xmmsc_next_id ();

	xmmsc_ipc_msg_write (c->ipc, msg, cid);

	return xmmsc_result_new (c, cid);
}

x_hash_t *
xmmsc_deserialize_hashtable (xmms_ipc_msg_t *msg)
{
	unsigned int entries;
	unsigned int i;
	unsigned int len;
	x_hash_t *h;
	char *key, *val;

	if (!xmms_ipc_msg_get_uint32 (msg, &entries))
		return NULL;

	h = x_hash_new_full (x_str_hash, x_str_equal, g_free, g_free);

	for (i = 1; i <= entries; i++) {
		if (!xmms_ipc_msg_get_string_alloc (msg, &key, &len))
			goto err;
		if (!xmms_ipc_msg_get_string_alloc (msg, &val, &len))
			goto err;

		x_hash_insert (h, key, val);
	}

	return h;

err:
	x_hash_destroy (h);
	return NULL;

}

void xmms_log_debug (const gchar *fmt, ...)
{
	char buff[1024];
	va_list ap;

	va_start (ap, fmt);
#ifdef HAVE_VSNPRINTF
	vsnprintf (buff, 1024, fmt, ap);
#else
	vsprintf (buff, fmt, ap);
#endif
	va_end (ap);

	fprintf (stderr, "%s\n", buff);
}

