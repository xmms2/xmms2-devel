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
#include <dbus/dbus-glib.h>
#include <glib.h>

#include "xmmsclient.h"
#include "xmms/signal_xmms.h"

#define XMMS_MAX_URI_LEN 1024

typedef enum {
	XMMSC_TYPE_UINT32,
	XMMSC_TYPE_STRING,
	XMMSC_TYPE_NONE,
	XMMSC_TYPE_VIS,
	XMMSC_TYPE_MOVE,
} xmmsc_types_t;

typedef struct xmmsc_signal_callbacks_St {
	gchar *signal_name;
	xmmsc_types_t type;
} xmmsc_signal_callbacks_t;

struct xmmsc_connection_St {
	DBusConnection *conn;	
	gchar *error;
	GHashTable *callbacks;
};

typedef struct xmmsc_callback_desc_St {
	void (*func)(void *, void *);
	void *userdata;
} xmmsc_callback_desc_t;


static xmmsc_signal_callbacks_t callbacks[] = {
	{ XMMS_SIGNAL_PLAYBACK_PLAYTIME, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_CORE_INFORMATION, XMMSC_TYPE_STRING },
	{ XMMS_SIGNAL_PLAYBACK_STOP, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYBACK_CURRENTID, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_CORE_DISCONNECT, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_ADD, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_PLAYLIST_SHUFFLE, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_CLEAR, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_REMOVE, XMMSC_TYPE_NONE },
	{ XMMS_SIGNAL_PLAYLIST_JUMP, XMMSC_TYPE_UINT32 },
	{ XMMS_SIGNAL_PLAYLIST_MOVE, XMMSC_TYPE_MOVE },
	{ XMMS_SIGNAL_VISUALISATION_SPECTRUM, XMMSC_TYPE_VIS },
	{ NULL, 0 },
};


/*
 * Forward declartions
 */

static xmmsc_signal_callbacks_t *get_callback (const gchar *signal);
static DBusHandlerResult handle_callback (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, void *user_data);
static void xmmsc_register_signal (xmmsc_connection_t *conn, gchar *signal);
static void xmmsc_send_void (xmmsc_connection_t *c, char *message);

/*
 * Public methods
 */


void
xmmsc_quit (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_SIGNAL_CORE_QUIT);
}

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

void
xmmsc_play_next (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_SIGNAL_PLAYBACK_NEXT);
}

void
xmmsc_play_prev (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYBACK_PREV);
}

void
xmmsc_playlist_shuffle (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYLIST_SHUFFLE);
}

void
xmmsc_playlist_clear (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYLIST_CLEAR);
}

void
xmmsc_playback_stop (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYBACK_STOP);
}

void
xmmsc_playback_start (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,XMMS_SIGNAL_PLAYBACK_PLAY);
}

void
xmmsc_playlist_jump (xmmsc_connection_t *c, guint id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_JUMP, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

}

void
xmmsc_playback_seek (xmmsc_connection_t *c, guint milliseconds)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYBACK_SEEK, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, milliseconds);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

void
xmmsc_playlist_add (xmmsc_connection_t *c, char *uri)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_ADD, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, uri);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

}

void
xmmsc_playlist_remove (xmmsc_connection_t *c, guint id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_REMOVE, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

}

GList *
xmmsc_playlist_list (xmmsc_connection_t *c)
{
	GList *list=NULL;
	DBusMessage *msg,*res;
	DBusError err;
	
	dbus_error_init (&err);
	
	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_LIST, NULL);
	res = dbus_connection_send_with_reply_and_block (c->conn, msg, 2000, &err);
	if (res) {
		DBusMessageIter itr;
		
		dbus_message_iter_init (res, &itr);
		while (42) {
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
				xmmsc_playlist_entry_t *entry;
				entry = calloc(1,sizeof(xmmsc_playlist_entry_t));
			        entry->id = dbus_message_iter_get_uint32 (&itr);
				entry->properties = xmmsc_playlist_get_mediainfo (c, entry->id);
				entry->url = (gchar *)g_hash_table_lookup (entry->properties, "uri");
				list = g_list_append (list, entry);

			}
			if (!dbus_message_iter_has_next (&itr)){
				break;
			}
			dbus_message_iter_next (&itr);
		}
		dbus_message_unref (res);
	} else {
		 g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "no response?\n");
	}
	
	dbus_message_unref (msg);
	return list;

}


guint
xmmsc_get_playing_id (xmmsc_connection_t *c)
{
	DBusMessage *res;
	DBusMessage *msg = dbus_message_new (XMMS_SIGNAL_PLAYBACK_CURRENTID, NULL);
	DBusError err;
	guint id = 0;

	dbus_error_init (&err);
	
	res = dbus_connection_send_with_reply_and_block (c->conn, msg, 2000, &err);

	if (res) {
		DBusMessageIter itr;
		
		dbus_message_iter_init (res, &itr);
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
			id = dbus_message_iter_get_uint32 (&itr);
		} else {
			g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "bad response :(\n");
		}
	}

	dbus_message_unref (msg);

	return id;
}

GHashTable *
xmmsc_playlist_get_mediainfo (xmmsc_connection_t *c, guint id)
{
	DBusMessage *msg, *ret;
	DBusMessageIter itr;
	DBusError err;
	GHashTable *tab = NULL;

	dbus_error_init (&err);

	msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_MEDIAINFO, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	ret = dbus_connection_send_with_reply_and_block (c->conn, msg, 2000, &err);
	if (ret) {
		DBusMessageIter itr;
		
		dbus_message_iter_init (ret, &itr);
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_DICT) {
			DBusMessageIter dictitr;
			dbus_message_iter_init_dict_iterator (&itr, &dictitr);

			tab = g_hash_table_new (g_str_hash, g_str_equal);

			while (42) {
				g_hash_table_insert (tab, dbus_message_iter_get_dict_key (&dictitr), dbus_message_iter_get_string (&dictitr));
				if (!dbus_message_iter_has_next (&dictitr))
					break;
				dbus_message_iter_next (&dictitr);
			}

		} else {
			g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "bad response :(\n");
		}
	}

	dbus_message_unref (msg);
	dbus_connection_flush (c->conn);

	return tab;
}

static gboolean
free_str (gpointer key, gpointer value, gpointer udata)
{
	if (key)
		g_free (key);
	if (value)
		g_free (value);

	return TRUE;
}

void
xmmsc_playlist_entry_free (GHashTable *entry)
{
	g_return_if_fail (entry);

	g_hash_table_foreach_remove (entry, free_str, NULL);

	g_hash_table_destroy (entry);
}



/*
 * Public utils
 */

xmmsc_connection_t *
xmmsc_init ()
{
	xmmsc_connection_t *c;
	
	c = g_new0 (xmmsc_connection_t,1);
	
	if (c) {
		c->callbacks = g_hash_table_new (g_str_hash,  g_str_equal);
	}

	return c;
}

gboolean
xmmsc_connect (xmmsc_connection_t *c)
{
	DBusConnection *conn;
	DBusError err;
	DBusMessageHandler *hand;
	gint i = 0;

	dbus_error_init (&err);

	conn = dbus_connection_open ("unix:path=/tmp/xmms-dbus", &err);
	
	if (!conn) {
		int ret;
		
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Couldn't connect to xmms2d, starning it...\n");
		ret = system ("xmms2d -d");

		if (ret != 0) {
			c->error = "Error starting xmms2d";
			return FALSE;
		}
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "started!\n");

		dbus_error_init (&err);
		conn = dbus_connection_open ("unix:path=/tmp/xmms-dbus", &err);
		if (!conn) {
			c->error = "Couldn't connect to xmms2d even tough I started it... Bad, very bad.";
			return FALSE;
		}
	}


	while (callbacks[i].signal_name) {
		hand = dbus_message_handler_new (handle_callback, c, NULL);
		dbus_connection_register_handler (conn, hand, &callbacks[i].signal_name, 1);

		i++;
	}
	
	c->conn=conn;
	return TRUE;
}

void
xmmsc_deinit (xmmsc_connection_t *c)
{
	dbus_connection_flush (c->conn);
	dbus_connection_disconnect (c->conn);
	dbus_connection_unref (c->conn);
}

void
xmmsc_set_callback (xmmsc_connection_t *conn, gchar *callback, void (*func)(void *,void*), void *userdata)
{
	xmmsc_callback_desc_t *desc = g_new0 (xmmsc_callback_desc_t, 1);

	desc->func = func;
	desc->userdata = userdata;

	xmmsc_register_signal (conn, callback);

	/** @todo more than one callback of each type */
	g_hash_table_insert (conn->callbacks, g_strdup (callback), desc);

}

void
xmmsc_glib_setup_mainloop (xmmsc_connection_t *conn, GMainContext *context)
{
	dbus_connection_setup_with_g_main (conn->conn, context);
}

#define _REGULARCHAR(a) ((a>=65 && a<=90) || (a>=97 && a<=122)) || (isdigit (a))

/* encode URL */
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

gchar *
xmmsc_get_last_error (xmmsc_connection_t *c)
{
	return c->error;
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
handle_callback (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, 
		void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_signal_callbacks_t *c;
	xmmsc_callback_desc_t *cb;
        DBusMessageIter itr;
	guint tmp[2]; /* used by MOVE */
	void *arg = NULL;

	c = get_callback (dbus_message_get_name (msg));

	/* This shouldnt happen, this means that we have registered a signal
	   there is no callback for. */
	if (!c) {
		return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
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
				dbus_message_iter_get_double_array (&itr, (double *) &arg, &len);
			}
			break;

		case XMMSC_TYPE_MOVE:
			if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
				tmp[0] = dbus_message_iter_get_uint32 (&itr);
				tmp[1] = dbus_message_iter_get_uint32 (&itr);
				arg = &tmp;
			}
			break;

		case XMMSC_TYPE_NONE:
			/* don't really know how to handle this yet. */
			break;
	}

	cb = g_hash_table_lookup (xmmsconn->callbacks, dbus_message_get_name (msg));
	if(cb){
		cb->func(cb->userdata, arg);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void
xmmsc_register_signal (xmmsc_connection_t *conn, gchar *signal)
{
	DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (XMMS_SIGNAL_CORE_SIGNAL_REGISTER, NULL);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, signal);
	dbus_connection_send (conn->conn, msg, &cserial);
	dbus_message_unref (msg);

}

static void
xmmsc_send_void (xmmsc_connection_t *c, char *message)
{
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new (message, NULL);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}


