/**
 *
 */

/* YES! I know that this api may change under my feet */
#define DBUS_API_SUBJECT_TO_CHANGE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

#include "xmmsclient.h"
#include "xmms/signal_xmms.h"

guint played_time=0;

#define XMMS_MAX_URI_LEN 1024

static const char *playtime_message[]={XMMS_SIGNAL_PLAYBACK_PLAYTIME};
static const char *information_message[]={XMMS_SIGNAL_CORE_INFORMATION};
static const char *playback_stopped_message[]={XMMS_SIGNAL_PLAYBACK_STOP};
static const char *mediainfo_message[]={XMMS_SIGNAL_PLAYBACK_CURRENTID};
static const char *disconnectmsgs[]={XMMS_SIGNAL_CORE_DISCONNECT};

static const char *playlist_added[] = {XMMS_SIGNAL_PLAYLIST_ADD};
static const char *playlist_shuffled[] = {XMMS_SIGNAL_PLAYLIST_SHUFFLE};
static const char *playlist_cleared[] = {XMMS_SIGNAL_PLAYLIST_CLEAR};
static const char *playlist_removed[] = {XMMS_SIGNAL_PLAYLIST_REMOVE};
static const char *playlist_jumped[] = {XMMS_SIGNAL_PLAYLIST_JUMP};
static const char *playlist_moved[] = {XMMS_SIGNAL_PLAYLIST_MOVE};


struct xmmsc_connection_St {
	DBusConnection *conn;	
	gchar *error;
	GHashTable *callbacks;
};

typedef struct xmmsc_callback_desc_St {
	void (*func)(void *, void *);
	void *userdata;
} xmmsc_callback_desc_t;

gchar *
xmmsc_get_last_error (xmmsc_connection_t *c)
{
	return c->error;
}

static DBusHandlerResult
handle_playtime(DBusMessageHandler *handler, 
	   DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
        DBusMessageIter itr;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
		guint tme = dbus_message_iter_get_uint32 (&itr);
		xmmsc_callback_desc_t *cb;

		cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_PLAYTIME_CHANGED);

		if(cb){
			cb->func(cb->userdata, GUINT_TO_POINTER (tme));
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_information(DBusMessageHandler *handler, 
	   DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
        DBusMessageIter itr;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
		gchar *info = dbus_message_iter_get_string (&itr); 
		xmmsc_callback_desc_t *cb;

		cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_INFORMATION);

		if(cb){
			cb->func(cb->userdata, info);
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}


static DBusHandlerResult
handle_mediainfo(DBusMessageHandler *handler, 
	   DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
        DBusMessageIter itr;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
		guint id = dbus_message_iter_get_uint32 (&itr);
		xmmsc_callback_desc_t *cb;

		cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_MEDIAINFO_CHANGED);

		if(cb){
			cb->func(cb->userdata, GUINT_TO_POINTER (id));
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playback_stopped (DBusMessageHandler *handler, 
	   DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_callback_desc_t *cb;

	cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_PLAYBACK_STOPPED);

	if (cb) {
		cb->func (cb->userdata, NULL);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_disconnect (DBusMessageHandler *handler, 
	   DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_callback_desc_t *cb;

	cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_DISCONNECTED);

	if (cb) {
		cb->func (cb->userdata, NULL);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}


static DBusHandlerResult
handle_playlist_cleared (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, void *user_data)
{

	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_callback_desc_t *cb;

	cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_PLAYLIST_CLEARED);

	if (cb) {
		cb->func (cb->userdata, NULL);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_shuffled (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_callback_desc_t *cb;

	cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_PLAYLIST_SHUFFLED);

	if (cb) {
		cb->func (cb->userdata, NULL);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_jumped (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_callback_desc_t *cb;
	DBusMessageIter itr;

	cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_PLAYLIST_JUMPED);

	dbus_message_iter_init (msg, &itr);
	if (cb) {
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
			guint id = dbus_message_iter_get_uint32 (&itr);
			cb->func (cb->userdata, GUINT_TO_POINTER (id));
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_removed (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_callback_desc_t *cb;
	DBusMessageIter itr;

	cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_PLAYLIST_REMOVED);

	dbus_message_iter_init (msg, &itr);
	if (cb) {
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
			guint id = dbus_message_iter_get_uint32 (&itr);
			cb->func (cb->userdata, GUINT_TO_POINTER (id));
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_moved (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_callback_desc_t *cb;
	DBusMessageIter itr;

	cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_PLAYLIST_MOVED);

	dbus_message_iter_init (msg, &itr);

	if (cb) {
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
			guint id = dbus_message_iter_get_uint32 (&itr);
			guint newpos = dbus_message_iter_get_uint32 (&itr);
			guint foo[] = {id, newpos};

			cb->func (cb->userdata, (void *)foo);
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_added (DBusMessageHandler *handler, 
		DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	xmmsc_connection_t *xmmsconn = (xmmsc_connection_t *) user_data;
	xmmsc_callback_desc_t *cb;
	DBusMessageIter itr;

	cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_PLAYLIST_ADDED);

	dbus_message_iter_init (msg, &itr);

	if (cb) {
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
			guint id = dbus_message_iter_get_uint32 (&itr);
			guint options = dbus_message_iter_get_uint32 (&itr);
			guint foo[] = {id, options};

			cb->func (cb->userdata, (void *)foo);
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

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
	hand = dbus_message_handler_new (handle_playtime, c, NULL);
	dbus_connection_register_handler (conn, hand, playtime_message, 1);

	hand = dbus_message_handler_new (handle_mediainfo, c, NULL);
	dbus_connection_register_handler (conn, hand, mediainfo_message, 1);
	
	hand = dbus_message_handler_new (handle_playback_stopped, c, NULL);
	dbus_connection_register_handler (conn, hand, playback_stopped_message, 1);
	
	hand = dbus_message_handler_new (handle_disconnect, c, NULL);
	dbus_connection_register_handler (conn, hand, disconnectmsgs, 1);

	hand = dbus_message_handler_new (handle_information, c, NULL);
	dbus_connection_register_handler (conn, hand, information_message, 1);


	/* playlist messages */
	hand = dbus_message_handler_new (handle_playlist_added, c, NULL);
	dbus_connection_register_handler (conn, hand, playlist_added, 1);

	hand = dbus_message_handler_new (handle_playlist_shuffled, c, NULL);
	dbus_connection_register_handler (conn, hand, playlist_shuffled, 1);

	hand = dbus_message_handler_new (handle_playlist_removed, c, NULL);
	dbus_connection_register_handler (conn, hand, playlist_removed, 1);

	hand = dbus_message_handler_new (handle_playlist_cleared, c, NULL);
	dbus_connection_register_handler (conn, hand, playlist_cleared, 1);

	hand = dbus_message_handler_new (handle_playlist_jumped, c, NULL);
	dbus_connection_register_handler (conn, hand, playlist_jumped, 1);
	
	hand = dbus_message_handler_new (handle_playlist_moved, c, NULL);
	dbus_connection_register_handler (conn, hand, playlist_moved, 1);

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

void
xmmsc_set_callback (xmmsc_connection_t *conn, gchar *callback, void (*func)(void *,void*), void *userdata)
{
	xmmsc_callback_desc_t *desc = g_new0 (xmmsc_callback_desc_t, 1);

	desc->func = func;
	desc->userdata = userdata;

	if (g_strcasecmp (XMMSC_CALLBACK_PLAYTIME_CHANGED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYBACK_PLAYTIME);
	} else if (g_strcasecmp (XMMSC_CALLBACK_INFORMATION , callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_CORE_INFORMATION);
	} else if (g_strcasecmp (XMMSC_CALLBACK_MEDIAINFO_CHANGED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYBACK_CURRENTID);
	} else if (g_strcasecmp (XMMSC_CALLBACK_PLAYBACK_STOPPED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYBACK_STOP);
	} else if (g_strcasecmp (XMMSC_CALLBACK_DISCONNECTED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_CORE_DISCONNECT);

	} else if (g_strcasecmp (XMMSC_CALLBACK_PLAYLIST_CLEARED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYLIST_CLEAR);
	} else if (g_strcasecmp (XMMSC_CALLBACK_PLAYLIST_ADDED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYLIST_ADD);
	} else if (g_strcasecmp (XMMSC_CALLBACK_PLAYLIST_REMOVED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYLIST_REMOVE);
	} else if (g_strcasecmp (XMMSC_CALLBACK_PLAYLIST_SHUFFLED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYLIST_SHUFFLE);
	} else if (g_strcasecmp (XMMSC_CALLBACK_PLAYLIST_JUMPED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYLIST_JUMP);
	} else if (g_strcasecmp (XMMSC_CALLBACK_PLAYLIST_MOVED, callback) == 0) {
		xmmsc_register_signal (conn, XMMS_SIGNAL_PLAYLIST_MOVE);
	}

	/** @todo more than one callback of each type */
	g_hash_table_insert (conn->callbacks, g_strdup (callback), desc);

}

void
xmmsc_glib_setup_mainloop (xmmsc_connection_t *conn, GMainContext *context)
{
	dbus_connection_setup_with_g_main (conn->conn, context);
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


void
xmmsc_quit (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_SIGNAL_CORE_QUIT);
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

