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

#include "internal/xhash-int.h"
#include "internal/xlist-int.h"

#include "xmms/xmmsclient.h"
#include "xmms/signal_xmms.h"
#include "xmms/ipc.h"
#include "xmms/ipc_msg.h"
#include "xmms/signal_xmms.h"
#include "internal/client_ipc.h"

#include "internal/xmmsclient_int.h"

#define XMMS_MAX_URI_LEN 1024

#define _REGULARCHAR(a) ((a>=65 && a<=90) || (a>=97 && a<=122)) || (isdigit (a))

static void xmmsc_deinit (xmmsc_connection_t *c);

static uint32_t cmd_id;

/*
 * Public methods
 */

/**
 * @defgroup XMMSClient XMMSClient
 * @brief This functions will connect a client to a XMMS server.
 *
 * To connect to a XMMS server you first need to init the
 * xmmsc_connection_t and the connect it.
 * If you want to handle callbacks from the server (not only send
 * commands to it) you need to setup either a GMainLoop or QT-mainloop
 * with a XMMSWatch.
 * 
 * A GLIB example:
 * @code
 * int main () {
 *	xmmsc_connection_t *conn;
 *	GMainLoop *mainloop;
 *
 *	conn = xmmsc_init ();
 *	if (!xmmsc_connect (conn, NULL))
 *		return 0;
 *	
 *	mainloop = g_main_loop_new (NULL, FALSE);
 *	xmmsc_setup_with_gmain (conn, NULL);
 * }
 * @endcode
 *
 * And a QT example:
 * @code
 * int main (int argc, char **argv) {
 * 	XMMSClientQT *qtClient;
 *	xmmsc_connection_t *conn;
 *	QApplication app (argc, argv);
 *
 *	conn = xmmsc_init ();
 *	if (!xmmsc_connect (conn, NULL))
 *		return 0;
 *
 *	qtClient = new XMMSClientQT (conn, &app);
 *
 *	return app.exec ();	
 * }
 * @endcode
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

xmmsc_result_t *
xmmsc_send_hello (xmmsc_connection_t *c)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *result;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MAIN, XMMS_IPC_CMD_HELLO);
	xmms_ipc_msg_put_int32 (msg, 1); /* PROTOCOL VERSION */
	xmms_ipc_msg_put_string (msg, c->clientname);

	result = xmmsc_send_msg (c, msg);
	xmms_ipc_msg_destroy (msg);

	return result;
}

/**
 * Connects to the XMMS server.
 * If ipcpath is NULL, it will try to open the default path.
 * @todo document ipcpath.
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
		char *user = NULL;
		int uid = getuid();

		while ((pwd = getpwent ())) {
			if (uid == pwd->pw_uid) {
				user = pwd->pw_name;
				break;
			}
		}

		if (!user)
			return FALSE;

		snprintf (path, 256, "unix:///tmp/xmms-ipc-%s", user);
	} else {
		snprintf (path, 256, "%s", ipcpath);
	}

	ipc = xmmsc_ipc_init ();
	
	if (!xmmsc_ipc_connect (ipc, path)) {
		c->error = "Error starting xmms2d";
		return FALSE;

		/*
		ret = system ("xmms2d -d");

		if (ret != 0) {
			c->error = "Error starting xmms2d";
			return FALSE;
		}

		dbus_error_init (&err);
		conn = dbus_connection_open (path, &err);
		if (!conn) {
			c->error = "Couldn't connect to xmms2d even tough I started it... Bad, very bad.";
			return FALSE;
		}
		*/
	}

	c->ipc = ipc;

	result = xmmsc_send_hello (c);
	xmmsc_result_wait (result);
	ret = xmmsc_result_get_uint (result, &i);
	xmmsc_result_unref (result);

	return ret;
}

void
xmmsc_disconnect_callback_set (xmmsc_connection_t *c, void (*callback) (void*), void *userdata)
{
	xmmsc_ipc_disconnect_set (c->ipc, callback, userdata);
}


/**
 * Encodes a path for use with xmmsc_playlist_add
 *
 * @sa xmmsc_playlist_add
 */
char *
xmmsc_encode_path (char *path) {
	char *out, *outreal;
	int i;
	int len;

	len = strlen (path);
	outreal = out = (char *)calloc (1, len * 3 + 1);

	for ( i = 0; i < len; i++) {
		if (path[i] == '/' || 
			_REGULARCHAR ((int) path[i]) || 
			path[i] == '_' ||
			path[i] == '-' ){
			*(out++) = path[i];
		} else if (path[i] == ' '){
			*(out++) = '+';
		} else {
			snprintf (out, 4, "%%%02x", (unsigned char) path[i]);
			out += 3;
		}
	}

	return outreal;
}


/**
 * Returns a string that descibes the last error.
 */
char *
xmmsc_get_last_error (xmmsc_connection_t *c)
{
	return c->error;
}

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

/*
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
 * Decodes a path. All paths that are recived from
 * the server are encoded, to make them userfriendly
 * they need to be called with this function.
 *
 * @param path encoded path.
 *
 * @returns a char * that needs to be freed with free.
 */

char *
xmmsc_decode_path (const char *path)
{
	char *qstr;
	char tmp[3];
	int c1, c2;

	c1 = c2 = 0;

	qstr = (char *)calloc (1, strlen (path) + 1);

	tmp[2] = '\0';
	while (path[c1] != '\0'){
		if (path[c1] == '%'){
			int l;
			tmp[0] = path[c1+1];
			tmp[1] = path[c1+2];
			l = strtol(tmp,NULL,16);
			if (l!=0){
				qstr[c2] = (char)l;
				c1+=2;
			} else {
				qstr[c2] = path[c1];
			}
		} else if (path[c1] == '+') {
			qstr[c2] = ' ';
		} else {
			qstr[c2] = path[c1];
		}
		c1++;
		c2++;
	}
	qstr[c2] = path[c1];

	return qstr;
}

/**
 * This function will make a pretty string about the information in
 * the mediainfo hash supplied to it.
 * @param target a allocated char *
 * @param len length of target
 * @param fmt a format string to use
 * @param table the x_hash_t that you got from xmmsc_result_get_mediainfo
 * @returns the number of chars written to #target
 * @todo document format
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
	xmms_ipc_msg_destroy (msg);

	xmmsc_result_restartable (res, signalid);

	return res;
}

void
xmmsc_broadcast_disconnect (xmmsc_result_t *res)
{
	x_return_if_fail (res);

	/** @todo tell the server that we're not interested in the signal
	 *        any more.
	 */
	xmmsc_result_unref (res);
}

xmmsc_result_t *
xmmsc_send_signal_msg (xmmsc_connection_t *c, uint32_t signalid)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *res;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_SIGNAL, XMMS_IPC_CMD_SIGNAL);
	xmms_ipc_msg_put_uint32 (msg, signalid);
	
	res = xmmsc_send_msg (c, msg);
	xmms_ipc_msg_destroy (msg);
	
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
	xmms_ipc_msg_destroy (msg);

	return xmmsc_result_new (c, cid);
}

xmmsc_result_t *
xmmsc_send_msg (xmmsc_connection_t *c, xmms_ipc_msg_t *msg)
{
	uint32_t cid;
	cid = xmmsc_next_id ();

	xmmsc_ipc_msg_write (c->ipc, msg, cid);
	msg->cid = cid;

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

