/** @file
 * D-BUS handler.
 *
 * This code is responsible of setting up a D-BUS server that clients
 * can connect to. It serializes xmms-signals into dbus-messages that
 * are passed to the clients. The clients can send dbus-messages that
 * causes xmms-signals to be emitted.
 *
 * @url http://www.freedesktop.org/software/dbus/
 *
 */

/* YES! I know that this api may change under my feet */
#define DBUS_API_SUBJECT_TO_CHANGE

#include "util.h"
#include "playlist.h"
#include "core.h"

#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

static DBusServer *server;
static GMutex *connectionslock;
static GSList *connections = NULL;

static const char *messages[]={"org.xmms.core.mediainfo"};
static const char *nextmsgs[]={"org.xmms.core.play-next"};
static const char *prevmsgs[]={"org.xmms.core.play-prev"};
static const char *addmsgs[]={"org.xmms.playlist.add"};
static const char *mediainfomsgs[]={"org.xmms.playlist.mediainfo"};
static const char *listmsgs[]={"org.xmms.playlist.list"};
static const char *jumpmsgs[]={"org.xmms.playlist.jump"};
static const char *removemsgs[]={"org.xmms.playlist.remove"};
static const char *shufflemsgs[]={"org.xmms.playlist.shuffle"};
static const char *playmsgs[]={"org.xmms.playback.start"};
static const char *stopmsgs[]={"org.xmms.playback.stop"};
static const char *clearmsgs[]={"org.xmms.playlist.clear"};
static const char *quitmsgs[]={"org.xmms.core.quit"};
static const char *disconnectmsgs[]={"org.freedesktop.Local.Disconnect"};


static void do_send(gpointer data, gpointer user_data)
{
	 int clientser;
	 dbus_connection_send (data, user_data, &clientser);
}

static void
handle_playlist_changed_msg (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	const xmms_playlist_changed_msg_t *chmsg = data;
	DBusMessage *msg=NULL;
	DBusMessageIter itr;


	switch (chmsg->type) {
		case XMMS_PLAYLIST_CHANGED_ADD:
			msg = dbus_message_new ("org.xmms.playlist.added", NULL);
			dbus_message_append_iter_init (msg, &itr);
			dbus_message_iter_append_uint32 (&itr, chmsg->id);
			dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_INT (chmsg->arg));
			break;
		case XMMS_PLAYLIST_CHANGED_SHUFFLE:
			msg = dbus_message_new ("org.xmms.playlist.shuffled", NULL);
			break;
		case XMMS_PLAYLIST_CHANGED_CLEAR:
			msg = dbus_message_new ("org.xmms.playlist.cleared", NULL);
			break;
		case XMMS_PLAYLIST_CHANGED_REMOVE:
			msg = dbus_message_new ("org.xmms.playlist.removed", NULL);
			dbus_message_append_iter_init (msg, &itr);
			dbus_message_iter_append_uint32 (&itr, chmsg->id);
			break;
		case XMMS_PLAYLIST_CHANGED_SET_POS:
			msg = dbus_message_new ("org.xmms.playlist.jumped", NULL);
			dbus_message_append_iter_init (msg, &itr);
			dbus_message_iter_append_uint32 (&itr, chmsg->id);
			break;
		case XMMS_PLAYLIST_CHANGED_MOVE:
			msg = dbus_message_new ("org.xmms.playlist.moved", NULL);
			dbus_message_append_iter_init (msg, &itr);
			dbus_message_iter_append_uint32 (&itr, chmsg->id);
			dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_INT (chmsg->arg));
			break;
	}

	XMMS_DBG ("Sending playlist changed message: %s", dbus_message_get_name (msg));
	
	g_mutex_lock (connectionslock);

	if (connections) {
		g_slist_foreach (connections, do_send, msg);
	}

	g_mutex_unlock (connectionslock);

	dbus_message_unref (msg);
}

/** Send a dbus information message to all clients. */
static void
handle_information_msg (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	g_mutex_lock (connectionslock);

	if (connections) {
		DBusMessage *msg;
		DBusMessageIter itr;

		msg = dbus_message_new ("org.xmms.core.information", NULL);
		dbus_message_append_iter_init (msg, &itr);
		dbus_message_iter_append_string (&itr, (gchar *)data);
		g_slist_foreach (connections, do_send, msg);
		dbus_message_unref (msg);
	}

	g_mutex_unlock (connectionslock);

}

static void
handle_playback_stop (xmms_object_t *object, gconstpointer data, gpointer userdata)
{

	g_mutex_lock (connectionslock);

	if (connections) { 
		DBusMessage *msg;

		msg = dbus_message_new ("org.xmms.playback.stopped", NULL);
		g_slist_foreach (connections, do_send, msg);
		dbus_message_unref (msg);
	}


	g_mutex_unlock (connectionslock);

}

static void
handle_playtime_changed (xmms_object_t *object, gconstpointer data, gpointer userdata) 
{
	g_mutex_lock(connectionslock);

	if (connections) { 
		DBusMessage *msg;
		DBusMessageIter itr;

		msg = dbus_message_new ("org.xmms.core.playtime-changed", NULL);
		dbus_message_append_iter_init (msg, &itr);
		dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_UINT(data));
		g_slist_foreach (connections, do_send, msg);
		dbus_message_unref (msg);
	}

	 g_mutex_unlock(connectionslock);
}

static void
handle_mediainfo_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{

	 g_mutex_lock(connectionslock);

	if (connections) { 
		DBusMessage *msg;
		DBusMessageIter itr;

		msg = dbus_message_new ("org.xmms.core.mediainfo-changed", NULL);
		dbus_message_append_iter_init (msg, &itr);
		dbus_message_iter_append_uint32 (&itr, xmms_core_get_id ());
		g_slist_foreach (connections, do_send, msg);
		dbus_message_unref (msg);
	}

	 g_mutex_unlock(connectionslock);

}


static DBusHandlerResult
handle_mediainfo(DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	DBusMessage *rpy;
	DBusMessageIter itr;
	int clientser;
	gint id;
	
	XMMS_DBG ("mediainfo!");

	rpy = dbus_message_new_reply (msg);
	dbus_message_append_iter_init (rpy, &itr);
	id = xmms_core_get_id ();
	dbus_message_iter_append_uint32 (&itr, id);

	dbus_connection_send (conn, rpy, &clientser);
	dbus_message_unref (rpy);

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_quit(DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{

	XMMS_DBG ("quit!");
	xmms_core_quit();

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_next(DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{

	XMMS_DBG ("next!");
	xmms_core_play_next();

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_prev(DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{

	XMMS_DBG ("prev!");
	xmms_core_play_prev();

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_play(DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{

	XMMS_DBG ("play!");
	xmms_core_playback_start ();

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_stop(DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{

	XMMS_DBG ("stop!");
	xmms_core_playback_stop ();

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_clear (DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{

	XMMS_DBG ("clear!");
	xmms_core_playlist_clear ();

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_shuffle (DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{

	XMMS_DBG ("shuffle!");
	xmms_core_playlist_shuffle ();

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_add(DBusMessageHandler *handler, 
		    DBusConnection *conn, DBusMessage *msg, void *user_data)
{

        DBusMessageIter itr;

	XMMS_DBG ("playlistmsg!");

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
		gchar *uri = dbus_message_iter_get_string (&itr);
		XMMS_DBG ("adding to playlist: %s", uri);
		xmms_core_playlist_adduri (uri);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_list(DBusMessageHandler *handler, 
		     DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	DBusMessage *rpy;
	DBusMessageIter itr;
	xmms_playlist_t *playlist;
	GList *list,*save;
	int clientser;

	XMMS_DBG ("list!");

	rpy = dbus_message_new_reply (msg);
	dbus_message_append_iter_init (rpy, &itr);
	playlist = xmms_core_get_playlist ();
	save = list = xmms_playlist_list (playlist);

	while (list) {
		xmms_playlist_entry_t *entry=list->data;
		dbus_message_iter_append_uint32 (&itr, xmms_playlist_entry_id_get (entry));
		list = g_list_next (list);
	}

	dbus_connection_send (conn, rpy, &clientser);
	dbus_message_unref (rpy);

	g_list_free (save);

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}


static DBusHandlerResult
handle_disconnect (DBusMessageHandler *handler,
		   DBusConnection     *connection,
		   DBusMessage        *message,
                   void               *user_data)
{

	XMMS_DBG ("disconnect");

	g_mutex_lock(connectionslock);
	connections = g_slist_remove (connections, connection);
	g_mutex_unlock(connectionslock);

	dbus_connection_unref (connection);

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_jump (DBusMessageHandler *handler,
		   	DBusConnection     *connection,
			DBusMessage        *msg,
			void               *user_data)
{
        DBusMessageIter itr;

	XMMS_DBG ("jumpmsg!");

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
		guint id = dbus_message_iter_get_uint32 (&itr);
		XMMS_DBG ("Jumping to %d", id);
		xmms_core_playlist_jump (id);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_playlist_remove (DBusMessageHandler *handler,
		   	DBusConnection     *connection,
			DBusMessage        *msg,
			void               *user_data)
{
        DBusMessageIter itr;

	XMMS_DBG ("removemsg!");

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
		guint id = dbus_message_iter_get_uint32 (&itr);
		XMMS_DBG ("removing %d", id);
		xmms_core_playlist_remove (id);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static void
hash_to_dict (gpointer key, gpointer value, gpointer udata)
{
	gchar *k = key;
	gchar *v = value;
	DBusMessageIter *itr = udata;

	dbus_message_iter_append_dict_key (itr, k);
	dbus_message_iter_append_string (itr, v);
}

static DBusHandlerResult
handle_playlist_mediainfo (DBusMessageHandler *handler,
				DBusConnection     *connection,
				DBusMessage        *msg,
				void               *user_data)
{

	xmms_playlist_entry_t *entry=NULL;
	DBusMessageIter itr;
	DBusMessageIter dictitr;
	DBusMessage *reply=NULL;
	gint serial;

	XMMS_DBG ("playlist_mediainfomsg!");

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
		guint id = dbus_message_iter_get_uint32 (&itr);
		XMMS_DBG ("Getting info for %d", id);
		entry = xmms_core_playlist_entry_mediainfo (id);
	}

	if (entry) {
		gchar *uri = xmms_playlist_entry_get_uri (entry);
		reply = dbus_message_new_reply (msg);
		dbus_message_append_iter_init (reply, &itr);
		dbus_message_iter_append_dict (&itr, &dictitr);
		xmms_playlist_entry_foreach_prop (entry, hash_to_dict, &dictitr);
		if (uri) {
			dbus_message_iter_append_dict_key (&dictitr, "uri");
			dbus_message_iter_append_string (&dictitr, uri);
		}
		
	}

	if (reply) {
		dbus_connection_send (connection, reply, &serial);
		dbus_message_unref (reply);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}


static void
register_handler(DBusConnection *conn, DBusHandleMessageFunction func, const char **msgs, int n)
{
	DBusMessageHandler *hand;
	hand = dbus_message_handler_new (func, NULL, NULL);
	if (!hand) {
		XMMS_DBG ("couldn't alloc handler - bad");
		return;
	}
	dbus_connection_register_handler (conn, hand, msgs, n);
}


static void 
new_connect (DBusServer *server, DBusConnection *conn, void * data){

	XMMS_DBG ("new connection");

	dbus_connection_ref (conn);

	register_handler(conn,handle_mediainfo,messages,1);
	register_handler(conn,handle_playlist_add,addmsgs,1);
	register_handler(conn,handle_playlist_jump,jumpmsgs,1);
	register_handler(conn,handle_playlist_mediainfo,mediainfomsgs,1);
	register_handler(conn,handle_playlist_list,listmsgs,1);
	register_handler(conn,handle_playlist_shuffle,shufflemsgs,1);
	register_handler(conn,handle_playlist_clear,clearmsgs,1);
	register_handler(conn,handle_playlist_remove,removemsgs,1);
	register_handler(conn,handle_play,playmsgs,1);
	register_handler(conn,handle_stop,stopmsgs,1);
	register_handler(conn,handle_next,nextmsgs,1);
	register_handler(conn,handle_prev,prevmsgs,1);
	register_handler(conn,handle_quit,quitmsgs,1);
	register_handler(conn,handle_disconnect,disconnectmsgs,1);

	g_mutex_lock(connectionslock);
	connections = g_slist_prepend (connections, conn);
	g_mutex_unlock(connectionslock);

	dbus_connection_setup_with_g_main (conn, g_main_context_default());

}



/**
 * Initialize dbus.
 *
 * Creates a dbus server that waits for connections and registers
 * it with g_main.
 *
 * @returns TRUE if server successfully created, FALSE otherwise.
 *
 */
gboolean
xmms_dbus_init(){
        DBusError err;

	dbus_gthread_init (); //  dbus_enable_deadlocks ();

	connectionslock = g_mutex_new ();

        dbus_error_init (&err);

        server = dbus_server_listen ("unix:path=/tmp/xmms-dbus", &err);
        if (!server) {
                XMMS_DBG ("couldn't create server!\n");
                return FALSE;
        }
        dbus_server_ref (server);
	dbus_server_set_new_connection_function (server, new_connect,
						 NULL, NULL);
	
	dbus_server_setup_with_g_main (server, g_main_context_default());
	
	xmms_object_connect (XMMS_OBJECT (core), "playtime-changed",
			     handle_playtime_changed, NULL);
	
	xmms_object_connect (XMMS_OBJECT (core), "mediainfo-changed",
			     handle_mediainfo_changed, NULL);
	
	xmms_object_connect (XMMS_OBJECT (core), "playback-stopped",
			     handle_playback_stop, NULL);

	xmms_object_connect (XMMS_OBJECT (core), "information",
			     handle_information_msg, NULL);


	xmms_object_connect (XMMS_OBJECT (core), "playlist-changed",
			     handle_playlist_changed_msg, NULL);


	XMMS_DBG ("init done!");

        return TRUE;
}
