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

/* YES! I know that this api may change under my feet */
#define DBUS_API_SUBJECT_TO_CHANGE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <pwd.h>
#include <sys/types.h>

#include <dbus/dbus.h>
/*#include <glib.h>*/

#include "internal/xhash-int.h"
#include "internal/xlist-int.h"

#include "xmms/xmmsclient.h"
#include "xmms/signal_xmms.h"

#include "internal/xmmsclient_int.h"

#define XMMS_MAX_URI_LEN 1024

#define _REGULARCHAR(a) ((a>=65 && a<=90) || (a>=97 && a<=122)) || (isdigit (a))


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
 * @return a xmmsc_connection_t that should be freed with
 * xmmsc_deinit.
 *
 * @sa xmmsc_deinit
 */

xmmsc_connection_t *
xmmsc_init ()
{
	xmmsc_connection_t *c;
	
	c = malloc (sizeof (xmmsc_connection_t));
	
	if (c) {
		c->callbacks = x_hash_new (x_str_hash, x_str_equal);
		c->replies = x_hash_new (NULL, NULL);
	}
	
	if (!c->callbacks) {
		free (c);
		return NULL;
	}

	c->timeout = 2000; /* ms */

	return c;
}

/**
 * Connects to the XMMS server.
 * If dbuspath is NULL, it will try to open the default path.
 * @todo document dbuspath.
 *
 * @returns TRUE on success and FALSE if some problem
 * occured. call xmmsc_get_last_error to find out.
 *
 * @sa xmmsc_get_last_error
 */

int
xmmsc_connect (xmmsc_connection_t *c, const char *dbuspath)
{
	DBusConnection *conn;
	DBusError err;
	DBusObjectPathVTable vtable;

	char path[256];

	dbus_error_init (&err);

	if (!c)
		return FALSE;

	if (!dbuspath) {
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
			exit(-1);

		snprintf (path, 256, "unix:path=/tmp/xmms-dbus-%s", user);
	} else {
		snprintf (path, 256, "%s", dbuspath);
	}

	conn = dbus_connection_open (path, &err);
	
	if (!conn) {
		int ret;
		
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
	}

	memset (&vtable, 0, sizeof (vtable));

	/*dbus_connection_add_filter (conn, handle_callback, c, NULL);*/

	c->conn=conn;
	return TRUE;
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


/**
 * @internal
 */

static int
free_callback (void *key, void *value, void *udata)
{
	x_list_t *list = value;
	void *ldata;

	while (list) {
		ldata = list->data;
		list = x_list_delete_link (list, list);
		free (ldata);
	}

	free (key);

	return 1;
}


/**
 * Frees up any resources used by xmmsc_connection_t
 */

void
xmmsc_deinit (xmmsc_connection_t *c)
{
	dbus_connection_flush (c->conn);
	dbus_connection_disconnect (c->conn);
	dbus_connection_unref (c->conn);

	x_hash_foreach_remove (c->callbacks, free_callback, NULL);
	x_hash_destroy(c->callbacks);
	x_hash_destroy(c->replies);
	free(c);
}


/**
 * Tell the server to quit. This will terminate the server.
 * If you only want to disconnect, use #xmmsc_deinit()
 */

xmmsc_result_t *
xmmsc_quit (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_MAIN, XMMS_METHOD_QUIT);
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

static int
add (char *target, int max, int len, char *str)
{
	int tmpl;

	tmpl = strlen (str);

	if ((len + tmpl) > max)
		return 0;

	if (len == 0)
		strcpy (target, str);
	else 
		strcat (target, str);

	return tmpl;
}

static int
lookup_and_add (char *target, int max, int len, x_hash_t *table, const char *entry)
{
	char *tmp;
	int tmpl;

	tmp = x_hash_lookup (table, entry);
	if (!tmp)
		return 0;

	tmpl = add (target, max, len, tmp);

	return tmpl;
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
	int i = 0;
	char c;

	while (*fmt) {

		c = *(fmt++);

		if (c == '%') {
			switch (*fmt) {
				case 'a':
					/* artist */
					i += lookup_and_add (target, len, i, table, "artist");
					break;
				case 'b':
					/* album */
					i += lookup_and_add (target, len, i, table, "album");
					break;
				case 'c':
					/* channel name */
					i += lookup_and_add (target, len, i, table, "channel");
					break;
				case 'd':
					/* duration in ms */
					i += lookup_and_add (target, len, i, table, "duration");
					break;
				case 'g':
					/* genre */
					i += lookup_and_add (target, len, i, table, "genre");
					break;
				case 'h':
					/* samplerate */
					i += lookup_and_add (target, len, i, table, "samplerate");
					break;
				case 'u':
					/* URL */
					i += lookup_and_add (target, len, i, table, "url");
					break;
				case 'f':
					/* filename */
					{
						char *p;
						char *str;
						p = x_hash_lookup (table, "url");

						if (!p)
							continue;

					      	str = strrchr (p, '/');
						if (!str || !str[1])
							str = p;
						else
							str++;
						str = xmmsc_decode_path (str);

						i += add (target, len, i, str);
						free (str);
					}

					break;
				case 'o':
					/* comment */
					i += lookup_and_add (target, len, i, table, "comment");
					break;
				case 'm':
					{
						char *p;
						/* minutes */
					
						p = x_hash_lookup (table, "duration");
						if (!p) {
							i += add (target, len, i, "00");
						} else {
							int d = atoi (p);
							char t[4];
							snprintf (t, 4, "%02d", d/60000);
							i += add (target, len, i, t);
						}
						break;
					}
				case 'n':
					/* tracknr */
					i += lookup_and_add (target, len, i, table, "tracknr");
					break;
				case 'r':
					/* bitrate */
					i += lookup_and_add (target, len, i, table, "bitrate");
					break;
				case 's':
					{
						char *p;
						/* seconds */
					
						p = x_hash_lookup (table, "duration");
						if (!p) {
							i += add (target, len, i, "00");
						} else {
							int d = atoi (p);
							char t[4];
							snprintf (t, 4, "%02d", (d/1000)%60);
							i += add (target, len, i, t);
						}
						break;
					}

					break;
				case 't':
					/* title */
					i += lookup_and_add (target, len, i, table, "title");
					break;
				case 'y':
					/* year */
					i += lookup_and_add (target, len, i, table, "year");
					break;
			}
			*fmt++;
		} else {
			if (i < len) 
				target[i++] = c;
		}

	}

	target[len] = '\0';

	return i;

}


/** @} */


#define XMMSC_DEFAULT_TIMEOUT -1 /* Default DBUS behavior */

/**
 * @internal
 */

xmmsc_result_t *
xmmsc_send_msg_no_arg (xmmsc_connection_t *c, char *object, char *method)
{
	DBusMessage *msg;
	DBusPendingCall *pending;
	
	msg = dbus_message_new_method_call (NULL, object, XMMS_DBUS_INTERFACE, method);
	if (!dbus_connection_send_with_reply (c->conn, msg, &pending, XMMSC_DEFAULT_TIMEOUT)) {
		return NULL;
	}
	dbus_message_unref (msg);

	dbus_connection_flush (c->conn);

	return xmmsc_result_new (pending);

}

xmmsc_result_t *
xmmsc_send_msg (xmmsc_connection_t *c, DBusMessage *msg)
{
	DBusPendingCall *pending;
	
	if (!dbus_connection_send_with_reply (c->conn, msg, &pending, XMMSC_DEFAULT_TIMEOUT)) {
		return NULL;
	}
	
	dbus_connection_flush (c->conn);

	return xmmsc_result_new (pending);
}

xmmsc_result_t *
xmmsc_send_on_change (xmmsc_connection_t *c, DBusMessage *msg)
{
	DBusPendingCall *pending;
	
	if (!dbus_connection_send_with_reply (c->conn, msg, &pending, INT_MAX)) {
		return NULL;
	}
	
	dbus_connection_flush (c->conn);

	return xmmsc_result_new (pending);
}

x_hash_t *
xmmsc_deserialize_mediainfo (DBusMessageIter *itr)
{
	x_hash_t *tab = NULL;

	if (dbus_message_iter_get_arg_type (itr) == DBUS_TYPE_DICT) {
		DBusMessageIter dictitr;
		dbus_message_iter_init_dict_iterator (itr, &dictitr);
		
		tab = x_hash_new (x_str_hash, x_str_equal);
		
		while (42) {
			char *key = dbus_message_iter_get_dict_key (&dictitr);
			if (strcasecmp (key, "id") == 0) {
				x_hash_insert (tab, key, XUINT_TO_POINTER (dbus_message_iter_get_uint32 (&dictitr)));
			} else {
				x_hash_insert (tab, key, dbus_message_iter_get_string (&dictitr));
			}

			
			if (!dbus_message_iter_has_next (&dictitr))
				break;
			dbus_message_iter_next (&dictitr);
		}
	}
	return tab;
}
