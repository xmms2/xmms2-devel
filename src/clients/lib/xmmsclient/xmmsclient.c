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

guint played_time=0;

#define XMMS_MAX_URI_LEN 1024

static const char *playtime_message[]={"org.xmms.core.playtime-changed"};
static const char *information_message[]={"org.xmms.core.information"};
static const char *playback_stopped_message[]={"org.xmms.playback.stopped"};
static const char *mediainfo_message[]={"org.xmms.core.mediainfo-changed"};
static const char *disconnectmsgs[]={"org.freedesktop.Local.Disconnect"};


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
			cb->func(cb->userdata, GPOINTER_TO_UINT(tme));
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
		int id = dbus_message_iter_get_uint32 (&itr);
		xmmsc_callback_desc_t *cb;

		cb = g_hash_table_lookup (xmmsconn->callbacks, XMMSC_CALLBACK_MEDIAINFO_CHANGED);

		if(cb){
			cb->func(cb->userdata, GPOINTER_TO_UINT(id));
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
	xmmsc_send_void(c,"org.xmms.core.quit");
}

void
xmmsc_play_next (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,"org.xmms.core.play-next");
}

void
xmmsc_playlist_shuffle (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,"org.xmms.playlist.shuffle");
}

void
xmmsc_playlist_clear (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,"org.xmms.playlist.clear");
}

void
xmmsc_playback_stop (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,"org.xmms.playback.stop");
}

void
xmmsc_playback_start (xmmsc_connection_t *c)
{
	xmmsc_send_void(c,"org.xmms.playback.start");
}

void
xmmsc_playlist_jump (xmmsc_connection_t *c, guint id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new ("org.xmms.playlist.jump", NULL);
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
	
	msg = dbus_message_new ("org.xmms.playlist.add", NULL);
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
	
	msg = dbus_message_new ("org.xmms.playlist.remove", NULL);
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
	
	msg = dbus_message_new ("org.xmms.playlist.list", NULL);
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
	DBusMessage *msg = dbus_message_new ("org.xmms.core.mediainfo", NULL);
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

	msg = dbus_message_new ("org.xmms.playlist.mediainfo", NULL);
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

