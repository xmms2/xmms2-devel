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
#include <string.h>
#include <ctype.h>

#include <dbus/dbus.h>
#include <glib.h>

#include "xmms/xmmsclient.h"
#include "xmms/signal_xmms.h"

#include "internal/xmmsclient_int.h"

#define XMMS_MAX_URI_LEN 1024

#define _REGULARCHAR(a) ((a>=65 && a<=90) || (a>=97 && a<=122)) || (isdigit (a))


/**
 * @internal
 */

typedef enum {
	XMMSC_TYPE_UINT32,
	XMMSC_TYPE_STRING,
	XMMSC_TYPE_NONE,
	XMMSC_TYPE_VIS,
	XMMSC_TYPE_MOVE,
	XMMSC_TYPE_MEDIAINFO,
	XMMSC_TYPE_PLAYLIST,
	XMMSC_TYPE_TRANSPORT_LIST,
} xmmsc_types_t;


/**
 * @internal
 */

typedef struct xmmsc_signal_callbacks_St {
	gchar *signal_name;
	xmmsc_types_t type;
} xmmsc_signal_callbacks_t;


/**
 * @internal
 */

typedef struct xmmsc_callback_desc_St {
	void (*func)(void *, void *);
	void *userdata;
} xmmsc_callback_desc_t;


/**
 * @internal
 */

static xmmsc_signal_callbacks_t callbacks[] = {
	{ XMMS_SIGNAL_PLAYBACK_PLAYTIME, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_CORE_INFORMATION, XMMSC_TYPE_STRING },
	{ XMMS_SIGNAL_PLAYBACK_STATUS, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_PLAYBACK_CURRENTID, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_CORE_DISCONNECT, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_ADD, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_PLAYLIST_MEDIAINFO, XMMSC_TYPE_MEDIAINFO },
	{ XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_PLAYLIST_SHUFFLE, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_CLEAR, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_REMOVE, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_JUMP, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_PLAYLIST_MOVE, XMMSC_TYPE_MOVE },
	{ XMMS_SIGNAL_PLAYLIST_LIST, XMMSC_TYPE_PLAYLIST },
	{ XMMS_SIGNAL_PLAYLIST_SORT, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_VISUALISATION_SPECTRUM, XMMSC_TYPE_VIS },
	{ XMMS_SIGNAL_TRANSPORT_LIST, XMMSC_TYPE_TRANSPORT_LIST },
	{ NULL, 0 },
};


/*
 * Forward declartions
 */

static xmmsc_signal_callbacks_t *get_callback (const gchar *signal);
static DBusHandlerResult handle_callback (DBusConnection *conn, DBusMessage *msg, void *user_data);
static void xmmsc_register_signal (xmmsc_connection_t *conn, gchar *signal);

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
	
	c = g_new0 (xmmsc_connection_t,1);
	
	if (c) {
		c->callbacks = g_hash_table_new (g_str_hash,  g_str_equal);
		c->replies = g_hash_table_new (NULL,  NULL);
	}
	
	if (!c->callbacks) {
		g_free (c);
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

gboolean
xmmsc_connect (xmmsc_connection_t *c, const char *dbuspath)
{
	DBusConnection *conn;
	DBusError err;
	DBusObjectPathVTable vtable;

	const gchar *path;

	dbus_error_init (&err);

	if (!c)
		return FALSE;

	if (!dbuspath)
		path = g_strdup_printf ("unix:path=/tmp/xmms-dbus-%s", g_get_user_name ());
	else
		path = dbuspath;

	conn = dbus_connection_open (path, &err);
	
	if (!conn) {
		int ret;
		
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Couldn't connect to xmms2d, startning it...\n");
		ret = system ("xmms2d -d");

		if (ret != 0) {
			c->error = "Error starting xmms2d";
			return FALSE;
		}
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "started!\n");

		dbus_error_init (&err);
		conn = dbus_connection_open (path, &err);
		if (!conn) {
			c->error = "Couldn't connect to xmms2d even tough I started it... Bad, very bad.";
			return FALSE;
		}
	}

	memset (&vtable, 0, sizeof (vtable));

	dbus_connection_add_filter (conn, handle_callback, c, NULL);

	c->conn=conn;
	return TRUE;
}


/**
 * Encodes a path for use with xmmsc_playlist_add
 *
 * @sa xmmsc_playlist_add
 */
gchar *
xmmsc_encode_path (gchar *path) {
	gchar *out, *outreal;
	gint i;
	gint len;

	len = strlen (path);
	outreal = out = (gchar *)g_malloc0 (len * 3 + 1);

	for ( i = 0; i < len; i++) {
		if (path[i] == '/' || 
			_REGULARCHAR ((gint) path[i]) || 
			path[i] == '_' ||
			path[i] == '-' ){
			*(out++) = path[i];
		} else if (path[i] == ' '){
			*(out++) = '+';
		} else {
			g_snprintf (out, 4, "%%%02x", (guchar) path[i]);
			out += 3;
		}
	}

	return outreal;
}


/**
 * Returns a string that descibes the last error.
 */
gchar *
xmmsc_get_last_error (xmmsc_connection_t *c)
{
	return c->error;
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
}

/** 
 * @defgroup ClientCallbacks ClientCallbacks
 * @brief This callbacks can be set with xmmsc_set_callback
 * and will be called with one argument.
 * @{
 */

/**
 * @def XMMS_SIGNAL_PLAYBACK_PLAYTIME
 *
 * This callback is called after each sample
 * played by core. This should update the playtime.
 * The argument will be a UINT32 with the number of
 * milliseconds played. The latency is already accounted
 * for in the value you get.
 */

/**
 * @def XMMS_SIGNAL_PLAYBACK_STOP
 *
 * This callback is called after someone pressed
 * "stop". The core will remain "stopped" until 
 * XMMS_SIGNAL_PLAYBACK_CURRENTID is called. This
 * callback is called with %NULL argument.
 *
 * @sa xmmsc_playback_stop
 */

/**
 * @def XMMS_SIGNAL_PLAYBACK_CURRENTID
 *
 * This callback will be called when playback changed.
 * If playback is stopped this means that someone pressed
 * play. The argument to this function is a UINT32 with the
 * id of the song that is being played. Call xmmsc_playlist_mediainfo
 * to get the information on the song.
 *
 * @sa xmmsc_playlist_get_mediainfo
 * @sa xmmsc_playback_current_id
 */

/**
 * @def XMMS_SIGNAL_CORE_INFORMATION
 *
 * This callback will be called when there is information from
 * the server that should be presented to the user.
 * The argument is a zero terminated string.
 */

/**
 * @def XMMS_SIGNAL_CORE_DISCONNECT
 *
 * This callback will be called when the server wants us
 * to disconnect. This usely means that server exits.
 * Argument is %NULL.
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_ADD
 *
 * This callback will be called when something is added to
 * the playlist. 
 * 
 * The argument is a UINT32 with the id of the added song.
 *
 * @sa xmmsc_playlist_add
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_MEDIAINFO
 *
 * This will be called with information about a specific
 * song. The arugment is a pointer to GHashTable with 
 * information about the song.
 *
 * @sa xmmsc_playlist_get_mediainfo
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID
 *
 * This will be called when information for a specific
 * entry has been changed. The argument is a UINT32 with
 * the id of the changed entry.
 *
 * @sa xmmsc_playlist_get_mediainfo
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_SHUFFLE
 *
 * This is called when the playlist is shuffled.
 * You will have to reread the playlist with xmmsc_playlist_list.
 * The callback is called with %NULL as argument.
 *
 * @sa xmmsc_playlist_list
 * @sa xmmsc_playlist_shuffle
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_CLEAR
 *
 * This callback is called when the playlist is cleared.
 * 
 * @sa xmmsc_playlist_clear
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_REMOVE
 *
 * This is called when a song is removed from the playlist.
 * The argument is a UINT32 with the removed songs id.
 *
 * @sa xmmsc_playlist_remove
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_JUMP
 *
 * This is called when someone moves the current song pointer
 * in the playlist. 
 * The argument is a UINT32 with the new current entry.
 *
 * @sa xmmsc_playlist_jump
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_MOVE
 *
 * This is called when someone is moving something in the playlist
 * @todo figure out how the argument should work.
 *
 * @sa xmmsc_playlist_move
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_LIST
 *
 * This will be called with a list of ids that is in the playlist.
 * The argument is a UINT32 Array that will be zeroterminated.
 *
 * @sa xmmsc_playlist_list
 */

/**
 * @def XMMS_SIGNAL_PLAYLIST_SORT
 *
 * This will be called when someone has sorted the playlist.
 * You will have to reread the playlist to ensure you have
 * the correct data.
 *
 * @sa xmmsc_playlist_sort
 */

/**
 * @def XMMS_SIGNAL_VISUALISATION_SPECTRUM
 *
 * This will be called for each sample with the fft data for that
 * sample. The argument is a array with 10 doubles. This can be
 * used for output a spectrumanalyzer.
 */

/**
 * @def XMMS_SIGNAL_TRANSPORT_LIST
 *
 * This will be called when a file list is ready.
 * This is generated by a xmmsc_file_list call.
 *
 * @sa xmmsc_file_list
 */

/**
 * Set a callback for the signal you want to handle.
 * the callback function should have the following prototype
 * void callback (void *userdata, void *callbackdata)
 * What callbackdata points to is diffrent for each callback.
 */

void
xmmsc_set_callback (xmmsc_connection_t *conn, 
		    gchar *callback, 
		    void (*func) (void *, void*), 
		    void *userdata)
{
	GList *l = NULL;
	xmmsc_callback_desc_t *desc = g_new0 (xmmsc_callback_desc_t, 1);

	desc->func = func;
	desc->userdata = userdata;

	/** @todo more than one callback of each type */
	l = g_hash_table_lookup (conn->callbacks, callback);
	if (!l) {
		xmmsc_register_signal (conn, callback);
		l = g_list_append (l, desc);
		g_hash_table_insert (conn->callbacks, g_strdup (callback), l);
	} else {
		l = g_list_append (l, desc);
	}

}

/** @} */

/**
 * Disconnects you from the current XMMS server.
 */

void
xmmsc_quit (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_OBJECT_CORE, XMMS_METHOD_QUIT);
}

/**
 * Decodes a path. All paths that are recived from
 * the server are encoded, to make them userfriendly
 * they need to be called with this function.
 *
 * @param path encoded path.
 *
 * @returns a gchar * that needs to be freed with g_free.
 */

gchar *
xmmsc_decode_path (const gchar *path)
{
	gchar *qstr;
	gchar tmp[3];
	gint c1, c2;

	c1 = c2 = 0;

	qstr = (gchar *)g_malloc0 (strlen (path) + 1);

	tmp[2] = '\0';
	while (path[c1] != '\0'){
		if (path[c1] == '%'){
			gint l;
			tmp[0] = path[c1+1];
			tmp[1] = path[c1+2];
			l = strtol(tmp,NULL,16);
			if (l!=0){
				qstr[c2] = (gchar)l;
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
lookup_and_add (char *target, int max, int len, GHashTable *table, const char *entry)
{
	char *tmp;
	int tmpl;

	tmp = g_hash_table_lookup (table, entry);
	if (!tmp)
		return 0;

	tmpl = add (target, max, len, tmp);

	return tmpl;
}

/**
 * @todo Document this 
 */

int
xmmsc_entry_format (char *target, int len, const char *fmt, GHashTable *table)
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
						p = g_hash_table_lookup (table, "url");

						if (!p)
							continue;

					      	str = strrchr (p, '/');
						if (!str || !str[1])
							str = p;
						else
							str++;
						str = xmmsc_decode_path (str);

						i += add (target, len, i, str);
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
					
						p = g_hash_table_lookup (table, "duration");
						if (!p) {
							i += add (target, len, i, "00");
						} else {
							int d = atoi (p);
							char *t = g_strdup_printf ("%02d", d/60000);
							i += add (target, len, i, t);
							g_free (t);
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
					
						p = g_hash_table_lookup (table, "duration");
						if (!p) {
							i += add (target, len, i, "00");
						} else {
							int d = atoi (p);
							char *t = g_strdup_printf ("%02d", (d/1000)%60);
							i += add (target, len, i, t);
							g_free (t);
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
			target[i++] = c;
		}

	}

	return i;

}


/** @} */


/**
 * @internal
 */

int
xmmsc_send_void (xmmsc_connection_t *c, char *object, char *method)
{
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, object, XMMS_DBUS_INTERFACE, method);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
	return cserial;
}

void
xmmsc_connection_add_reply (xmmsc_connection_t *c, gint serial, gchar *type)
{
	g_hash_table_insert (c->replies, GUINT_TO_POINTER (serial), type);
}

GHashTable *
xmmsc_deserialize_mediainfo (DBusMessageIter *itr)
{
	GHashTable *tab = NULL;

	if (dbus_message_iter_get_arg_type (itr) == DBUS_TYPE_DICT) {
		DBusMessageIter dictitr;
		dbus_message_iter_init_dict_iterator (itr, &dictitr);
		
		tab = g_hash_table_new (g_str_hash, g_str_equal);
		
		while (42) {
			gchar *key = dbus_message_iter_get_dict_key (&dictitr);
			if (g_strcasecmp (key, "id") == 0) {
				g_hash_table_insert (tab, key, GUINT_TO_POINTER (dbus_message_iter_get_uint32 (&dictitr)));
			} else {
				g_hash_table_insert (tab, key, dbus_message_iter_get_string (&dictitr));
			}
			if (!dbus_message_iter_has_next (&dictitr))
				break;
			dbus_message_iter_next (&dictitr);
		}
	}
	return tab;
}

/*
 * Static utils
 */

static xmmsc_signal_callbacks_t *
get_callback (const gchar *signal)
{
	gint i = 0;

	while (callbacks[i].signal_name) {

		if (g_strcasecmp (callbacks[i].signal_name, signal) == 0)
			return &callbacks[i];
		i++;
	}

	return NULL;
}


static DBusHandlerResult
handle_callback (DBusConnection *conn, DBusMessage *msg, 
		void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_signal_callbacks_t *c;
	GList *cb_list;
	gchar msgname[256];
        DBusMessageIter itr;
	guint tmp[2]; /* used by MOVE */
	void *arg = NULL;


	/** allt det här apet hade vi gott kunna göra snyggare */
	g_snprintf (msgname, 255, "%s::%s", dbus_message_get_path (msg), dbus_message_get_member (msg));

	c = get_callback (msgname);

	if (!c) {
		gint rep_ser;

		rep_ser = dbus_message_get_reply_serial (msg);
		if (rep_ser > 0) {
			gchar *a;

			a = g_hash_table_lookup (xmmsconn->replies, GUINT_TO_POINTER (rep_ser));
			if (a != NULL) {
				g_hash_table_remove (xmmsconn->replies, GUINT_TO_POINTER (rep_ser));
				c = get_callback (a);
			}
			g_snprintf (msgname, 255, "%s", a);
		}

		if (c == NULL) {
			printf("\nNo callback for: %s -- %s -- %d\n", msgname, dbus_message_get_interface (msg), dbus_message_get_reply_serial (msg));
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
	}

	dbus_message_iter_init (msg, &itr);


	switch (c->type) {
		case XMMSC_TYPE_STRING:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING)
				arg = dbus_message_iter_get_string (&itr);
			break;
		case XMMSC_TYPE_UINT32:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32)
				arg = GUINT_TO_POINTER (dbus_message_iter_get_uint32 (&itr));
			break;
		case XMMSC_TYPE_VIS:
			{
				int len=0;
				dbus_message_iter_get_double_array (&itr, (double **) &arg, &len);
			}
			break;

		case XMMSC_TYPE_MOVE:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
				tmp[0] = dbus_message_iter_get_uint32 (&itr);
				tmp[1] = dbus_message_iter_get_uint32 (&itr);
				arg = &tmp;
			}
			break;

		case XMMSC_TYPE_PLAYLIST:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
				guint len = dbus_message_iter_get_uint32 (&itr);
				if (len > 0) {
					guint32 *arr;
					gint len;
					guint32 *tmp;

					dbus_message_iter_next (&itr);
					dbus_message_iter_get_uint32_array (&itr, &tmp, &len);

					arr = g_new0 (guint32, len+1);
					memcpy (arr, tmp, len * sizeof(guint32));
					arr[len] = '\0';
					
					arg = arr;
				}
			}
			break;
		case XMMSC_TYPE_MEDIAINFO:
			arg = xmmsc_deserialize_mediainfo (&itr);
			break;

		case XMMSC_TYPE_TRANSPORT_LIST:
			{
				GList *list = NULL;

				while (42) {
					if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
						xmmsc_file_t *f;
					
						f = g_new (xmmsc_file_t, 1);
					
						f->path = dbus_message_iter_get_string (&itr);
						dbus_message_iter_next (&itr);
						f->file = dbus_message_iter_get_boolean (&itr);
						list = g_list_append (list, f);
					}

					if (!dbus_message_iter_has_next (&itr))
						break;

					dbus_message_iter_next (&itr);
				}

				arg = list;
			}

			break;

		case XMMSC_TYPE_NONE:
			/* don't really know how to handle this yet. */
			break;
	}

	cb_list = g_hash_table_lookup (xmmsconn->callbacks, msgname);
	if (cb_list) {
		GList *node;
		for (node = cb_list; node; node = g_list_next (node)) {
			xmmsc_callback_desc_t *cb = node->data;
			cb->func(cb->userdata, arg);
		}
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}


static void
xmmsc_register_signal (xmmsc_connection_t *conn, gchar *signal)
{
	DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_CLIENT, XMMS_DBUS_INTERFACE, XMMS_METHOD_REGISTER);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, signal);
	dbus_connection_send (conn->conn, msg, &cserial);
	dbus_message_unref (msg);

}


