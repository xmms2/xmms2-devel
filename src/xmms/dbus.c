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
#include "signal_xmms.h"
#include "visualisation.h"

#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

static DBusServer *server;
static GMutex *connectionslock;
static GSList *connections = NULL;

/** Ids in signal mask */
typedef enum {
	XMMS_SIGNAL_MASK_PLAYBACK_PLAY = 1 << 0,
	XMMS_SIGNAL_MASK_PLAYBACK_STOP = 1 << 1,
	XMMS_SIGNAL_MASK_PLAYBACK_PAUSE = 1 << 2,
	XMMS_SIGNAL_MASK_PLAYBACK_NEXT = 1 << 3,
	XMMS_SIGNAL_MASK_PLAYBACK_PREV = 1 << 4,
	XMMS_SIGNAL_MASK_PLAYBACK_SEEK = 1 << 5,
	XMMS_SIGNAL_MASK_PLAYBACK_CURRENTID = 1 << 6,
	XMMS_SIGNAL_MASK_PLAYBACK_PLAYTIME = 1 << 7,

	XMMS_SIGNAL_MASK_PLAYLIST_ADD = 1 << 8,
	XMMS_SIGNAL_MASK_PLAYLIST_REMOVE = 1 << 9,
	XMMS_SIGNAL_MASK_PLAYLIST_LIST = 1 << 10,
	XMMS_SIGNAL_MASK_PLAYLIST_SHUFFLE = 1 << 11,
	XMMS_SIGNAL_MASK_PLAYLIST_CLEAR = 1 << 12,
	XMMS_SIGNAL_MASK_PLAYLIST_JUMP = 1 << 13,
	XMMS_SIGNAL_MASK_PLAYLIST_MEDIAINFO = 1 << 14,
	XMMS_SIGNAL_MASK_PLAYLIST_MOVE = 1 << 15,
	XMMS_SIGNAL_MASK_PLAYLIST_CHANGED = 1 << 16,

	XMMS_SIGNAL_MASK_CORE_QUIT = 1 << 17,
	XMMS_SIGNAL_MASK_CORE_DISCONNECT = 1 << 18,
	XMMS_SIGNAL_MASK_CORE_INFORMATION = 1 << 19,
	XMMS_SIGNAL_MASK_CORE_SIGNAL_REGISTER = 1 << 20,
	XMMS_SIGNAL_MASK_CORE_SIGNAL_UNREGISTER = 1 << 21,

	XMMS_SIGNAL_MASK_VISUALISATION_SPECTRUM = 1 << 22,
} xmms_dbus_signal_mask_t;


/* 
 * Forward declartions of all callbacks 
 */
static gboolean handle_playback_play ();
static gboolean handle_playback_stop ();
static gboolean handle_playback_pause ();
static gboolean handle_playback_next ();
static gboolean handle_playback_prev ();
static gboolean handle_playback_seek (DBusConnection *conn, DBusMessage *msg);
static gboolean handle_playback_currentid (DBusConnection *conn, DBusMessage *msg);
static gboolean handle_playlist_add (DBusConnection *conn, DBusMessage *msg);
static gboolean handle_playlist_remove (guint arg);
static gboolean handle_playlist_list (DBusConnection *conn, DBusMessage *msg);
static gboolean handle_playlist_shuffle ();
static gboolean handle_playlist_clear ();
static gboolean handle_playlist_jump (guint arg);
static gboolean handle_playlist_move (DBusConnection *conn, DBusMessage *msg);
static gboolean handle_playlist_mediainfo (DBusConnection *conn, DBusMessage *msg);
static gboolean handle_core_quit ();
static gboolean handle_core_disconnect (DBusConnection *conn, DBusMessage *msg);
static gboolean handle_core_signal_register (DBusConnection *conn, DBusMessage *msg);
static gboolean handle_core_signal_unregister (DBusConnection *conn, DBusMessage *msg);

static void send_playback_stop (xmms_object_t *object, 
	gconstpointer data, gpointer userdata);
static void send_playback_currentid (xmms_object_t *object, 
	gconstpointer data, gpointer userdata);
static void send_playback_playtime (xmms_object_t *object, 
	gconstpointer data, gpointer userdata);
static void send_core_information (xmms_object_t *object, 
	gconstpointer data, gpointer userdata);
static void send_playlist_changed (xmms_object_t *object, 
	gconstpointer data, gpointer userdata);
static void send_visualisation_spectrum (xmms_object_t *object,
					 gconstpointer data,
					 gpointer userdata);


typedef void (*xmms_dbus_object_callback_t) (xmms_object_t *object, gconstpointer data, gpointer userdata);

typedef gboolean (*xmms_dbus_callback_with_dbusmsg_t) (DBusConnection *conn, DBusMessage *msg);
typedef gboolean (*xmms_dbus_callback_with_noarg_t) ();
typedef gboolean (*xmms_dbus_callback_with_intarg_t) (guint arg);

typedef struct xmms_dbus_signal_mask_map_St {
	/** The dbus signalname, ie org.xmms.playback.play */
	gchar *dbus_name;
	xmms_dbus_signal_mask_t mask;

	/** This will be called when the xmms_name 
	  is received from the core object */
	xmms_dbus_object_callback_t object_callback;

	/** This will be called when the dbus_name 
	  is recieved from the client. */
	xmms_dbus_callback_with_dbusmsg_t callback_with_dbusmsg;
	xmms_dbus_callback_with_noarg_t callback_with_noarg;
	xmms_dbus_callback_with_intarg_t callback_with_intarg;
} xmms_dbus_signal_mask_map_t;

static xmms_dbus_signal_mask_map_t mask_map [] = {
	{ XMMS_SIGNAL_PLAYBACK_PLAY, 
		XMMS_SIGNAL_MASK_PLAYBACK_PLAY, 
		NULL, NULL, handle_playback_play, NULL },
	{ XMMS_SIGNAL_PLAYBACK_STOP, 
		XMMS_SIGNAL_MASK_PLAYBACK_STOP, 
		send_playback_stop, NULL, handle_playback_stop, NULL },
	{ XMMS_SIGNAL_PLAYBACK_PAUSE, 
		XMMS_SIGNAL_MASK_PLAYBACK_PAUSE, 
		NULL, NULL, handle_playback_pause, NULL },
	{ XMMS_SIGNAL_PLAYBACK_NEXT,
		XMMS_SIGNAL_MASK_PLAYBACK_NEXT, 
		NULL, NULL, handle_playback_next, NULL },
	{ XMMS_SIGNAL_PLAYBACK_PREV, 
		XMMS_SIGNAL_MASK_PLAYBACK_PREV, 
		NULL, NULL, handle_playback_prev, NULL },
	{ XMMS_SIGNAL_PLAYBACK_SEEK, 
		XMMS_SIGNAL_MASK_PLAYBACK_SEEK, 
		NULL, handle_playback_seek, NULL,  NULL },
	{ XMMS_SIGNAL_PLAYBACK_CURRENTID,
		XMMS_SIGNAL_MASK_PLAYBACK_CURRENTID, 
		send_playback_currentid, handle_playback_currentid, NULL, NULL },
	{ XMMS_SIGNAL_PLAYBACK_PLAYTIME,
		XMMS_SIGNAL_MASK_PLAYBACK_PLAYTIME, 
		send_playback_playtime, NULL, NULL, NULL },
	{ XMMS_SIGNAL_PLAYLIST_ADD,
		XMMS_SIGNAL_MASK_PLAYLIST_ADD, 
		NULL, handle_playlist_add, NULL, NULL },
	{ XMMS_SIGNAL_PLAYLIST_REMOVE,
		XMMS_SIGNAL_MASK_PLAYLIST_REMOVE, 
		NULL, NULL, NULL, handle_playlist_remove },
	{ XMMS_SIGNAL_PLAYLIST_LIST,
		XMMS_SIGNAL_MASK_PLAYLIST_LIST, 
		NULL, handle_playlist_list, NULL, NULL },
	{ XMMS_SIGNAL_PLAYLIST_SHUFFLE,
		XMMS_SIGNAL_MASK_PLAYLIST_SHUFFLE, 
		NULL, NULL, handle_playlist_shuffle, NULL },
	{ XMMS_SIGNAL_PLAYLIST_CLEAR,
		XMMS_SIGNAL_MASK_PLAYLIST_CLEAR, 
		NULL, NULL, handle_playlist_clear, NULL },
	{ XMMS_SIGNAL_PLAYLIST_JUMP,
		XMMS_SIGNAL_MASK_PLAYLIST_JUMP, 
		NULL, NULL, NULL, handle_playlist_jump },
	{ XMMS_SIGNAL_PLAYLIST_MEDIAINFO, 
		XMMS_SIGNAL_MASK_PLAYLIST_MEDIAINFO, 
		NULL, handle_playlist_mediainfo, NULL, NULL },
	{ XMMS_SIGNAL_PLAYLIST_MOVE,
		XMMS_SIGNAL_MASK_PLAYLIST_MOVE, 
		NULL, handle_playlist_move, NULL, NULL },
	{ XMMS_SIGNAL_PLAYLIST_CHANGED,
		XMMS_SIGNAL_MASK_PLAYLIST_CHANGED, 
		send_playlist_changed, NULL, NULL, NULL },
	{ XMMS_SIGNAL_CORE_QUIT,
		XMMS_SIGNAL_MASK_CORE_QUIT, 
		NULL, NULL, handle_core_quit, NULL },
	{ XMMS_SIGNAL_CORE_DISCONNECT,
		XMMS_SIGNAL_MASK_CORE_DISCONNECT, 
		NULL, handle_core_disconnect, NULL, NULL },
	{ XMMS_SIGNAL_CORE_INFORMATION, 
		XMMS_SIGNAL_MASK_CORE_INFORMATION, 
		send_core_information, NULL, NULL, NULL },
	{ XMMS_SIGNAL_CORE_SIGNAL_REGISTER,
		XMMS_SIGNAL_MASK_CORE_SIGNAL_REGISTER, 
		NULL, handle_core_signal_register, NULL, NULL },
	{ XMMS_SIGNAL_CORE_SIGNAL_UNREGISTER,
		XMMS_SIGNAL_MASK_CORE_SIGNAL_UNREGISTER, 
		NULL, handle_core_signal_unregister, NULL, NULL },
	{ XMMS_SIGNAL_VISUALISATION_SPECTRUM,
		XMMS_SIGNAL_MASK_VISUALISATION_SPECTRUM, 
		send_visualisation_spectrum, NULL, NULL, NULL },
};

typedef struct xmms_dbus_connection_St {
        gint signals;
        DBusConnection *connection;
} xmms_dbus_connection_t;

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
hash_to_dict (gpointer key, gpointer value, gpointer udata)
{
        gchar *k = key;
        gchar *v = value;
        DBusMessageIter *itr = udata;
 
        dbus_message_iter_append_dict_key (itr, k);
        dbus_message_iter_append_string (itr, v);
	
}

static xmms_dbus_signal_mask_map_t *
get_mask_map (const gchar *name)
{
	gint i=0;
	while (mask_map[i].dbus_name) {
		if (g_strcasecmp (mask_map[i].dbus_name, name) == 0)
			return &mask_map[i];
		i++;
	}
	return NULL;
}


static void 
do_send(gpointer data, gpointer user_data)
{
         int clientser;
         xmms_dbus_connection_t *conn = data;
	 const gchar *msg_name = dbus_message_get_name (user_data);
	 xmms_dbus_signal_mask_map_t *map = get_mask_map (msg_name);

	 if (conn->signals & map->mask)
         	dbus_connection_send (conn->connection, user_data, &clientser);
}


static xmms_dbus_connection_t *
get_conn (DBusConnection *conn)
{
	GSList *tmp;

        for (tmp = connections; tmp; tmp = g_slist_next (tmp)) {
		xmms_dbus_connection_t *c = tmp->data;
                if (c->connection == conn)
			return c;
        }
	return NULL;
}


/*
 * object callbacks
 */

static void
send_playlist_changed (xmms_object_t *object,
		gconstpointer data,
		gpointer userdata)
{

        const xmms_playlist_changed_msg_t *chmsg = data;
        DBusMessage *msg=NULL;
        DBusMessageIter itr;
                                                                                                    
                                                                                                    
        switch (chmsg->type) {
                case XMMS_PLAYLIST_CHANGED_ADD:
                        msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_ADD, NULL);
                        dbus_message_append_iter_init (msg, &itr);
                        dbus_message_iter_append_uint32 (&itr, chmsg->id);
                        dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_INT (chmsg->arg));
                        break;
                case XMMS_PLAYLIST_CHANGED_SHUFFLE:
                        msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_SHUFFLE, NULL);
                        break;
                case XMMS_PLAYLIST_CHANGED_CLEAR:
                        msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_CLEAR, NULL);
                        break;
                case XMMS_PLAYLIST_CHANGED_REMOVE:
                        msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_REMOVE, NULL);
                        dbus_message_append_iter_init (msg, &itr);
                        dbus_message_iter_append_uint32 (&itr, chmsg->id);
                        break;
                case XMMS_PLAYLIST_CHANGED_SET_POS:
                        msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_JUMP, NULL);
                        dbus_message_append_iter_init (msg, &itr);
                        dbus_message_iter_append_uint32 (&itr, chmsg->id);
                        break;
                case XMMS_PLAYLIST_CHANGED_MOVE:
                        msg = dbus_message_new (XMMS_SIGNAL_PLAYLIST_MOVE, NULL);
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

static void
send_playback_stop (xmms_object_t *object,
		gconstpointer data,
		gpointer userdata)
{
        g_mutex_lock (connectionslock);
                                                                                                    
        if (connections) {
                DBusMessage *msg;
                                                                                                    
                msg = dbus_message_new (XMMS_SIGNAL_PLAYBACK_STOP, NULL);
                g_slist_foreach (connections, do_send, msg);
                dbus_message_unref (msg);
        }
                                                                                                    
                                                                                                    
        g_mutex_unlock (connectionslock);
}

static void
send_playback_playtime (xmms_object_t *object,
		gconstpointer data,
		gpointer userdata)
{

        g_mutex_lock(connectionslock);
                                                                                                    
        if (connections) {
                DBusMessage *msg;
                DBusMessageIter itr;
                                                                                                    
                msg = dbus_message_new (XMMS_SIGNAL_PLAYBACK_PLAYTIME, NULL);
                dbus_message_append_iter_init (msg, &itr);
                dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_UINT(data));
                g_slist_foreach (connections, do_send, msg);
                dbus_message_unref (msg);
        }
                                                                                                    
	g_mutex_unlock(connectionslock);

}

static void
send_visualisation_spectrum (xmms_object_t *object,
		gconstpointer data,
		gpointer userdata)
{

        g_mutex_lock(connectionslock);

        if (connections) {
                DBusMessage *msg;
                DBusMessageIter itr;
		double val[FFT_LEN/2];
		float *fval = (float *)data;
		int i;

		for (i=0; i<FFT_LEN/2+1; i++) {
			val[i]=fval[i];
		}

                msg = dbus_message_new (XMMS_SIGNAL_VISUALISATION_SPECTRUM, NULL);
                dbus_message_append_iter_init (msg, &itr);
		dbus_message_iter_append_double_array (&itr, val, FFT_LEN/2+1);
                g_slist_foreach (connections, do_send, msg);
                dbus_message_unref (msg);
        }

	g_mutex_unlock(connectionslock);

}

static void
send_playback_currentid (xmms_object_t *object,
		gconstpointer data,
		gpointer userdata)
{

        g_mutex_lock(connectionslock);
                                                                                                    
        if (connections) {
                DBusMessage *msg;
                DBusMessageIter itr;
                                                                                                    
                msg = dbus_message_new (XMMS_SIGNAL_PLAYBACK_CURRENTID, NULL);
                dbus_message_append_iter_init (msg, &itr);
                dbus_message_iter_append_uint32 (&itr, xmms_core_get_id ());
                g_slist_foreach (connections, do_send, msg);
                dbus_message_unref (msg);
        }
                                                                                                    
        g_mutex_unlock(connectionslock);

}

static void
send_core_information (xmms_object_t *object,
		gconstpointer data,
		gpointer userdata)
{
        g_mutex_lock (connectionslock);
 
        if (connections) {
                DBusMessage *msg;
                DBusMessageIter itr;
 
                msg = dbus_message_new (XMMS_SIGNAL_CORE_INFORMATION, NULL);
                dbus_message_append_iter_init (msg, &itr);
                dbus_message_iter_append_string (&itr, (gchar *)data);
                g_slist_foreach (connections, do_send, msg);
                dbus_message_unref (msg);
        }
                                                                                                    
        g_mutex_unlock (connectionslock);
}

/*
 * dbus callbacks
 */

static gboolean
handle_core_signal_register (DBusConnection *conn, DBusMessage *msg)
{

	DBusMessageIter itr;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
		gchar *signal = dbus_message_iter_get_string (&itr);
		xmms_dbus_signal_mask_map_t *map = get_mask_map (signal);
		if (map) {
			xmms_dbus_connection_t *c;
			c = get_conn (conn);
			if (c)
				c->signals |= map->mask;
		}
	}

	return TRUE;
}

static gboolean
handle_core_signal_unregister (DBusConnection *conn, DBusMessage *msg)
{
	DBusMessageIter itr;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
		gchar *signal = dbus_message_iter_get_string (&itr);
		xmms_dbus_signal_mask_map_t *map = get_mask_map (signal);
		if (map) {
			xmms_dbus_connection_t *c;
			c = get_conn (conn);
			if (c)
				c->signals &= ~map->mask;
		}
	}

	return TRUE;

}


static gboolean
handle_playback_play ()
{
	xmms_core_playback_start ();

	return TRUE;
}

static gboolean
handle_playback_stop ()
{
	xmms_core_playback_stop ();

	return TRUE;
}

static gboolean
handle_playback_pause ()
{
	/** @todo add xmms_core_playback_pause */
	return TRUE;
}

static gboolean
handle_playback_next ()
{
	xmms_core_play_next ();
	return TRUE;
}

static gboolean
handle_playback_prev ()
{
	xmms_core_play_prev ();
	return TRUE;
}

static gboolean
handle_playback_seek (DBusConnection *conn, DBusMessage *msg)
{
	/** @todo make clients able to seek */

	return TRUE;
}

static gboolean
handle_playback_currentid (DBusConnection *conn, DBusMessage *msg)
{
        DBusMessage *rpy;
        DBusMessageIter itr;
        int clientser;
        gint id;
         
        rpy = dbus_message_new_reply (msg);
        dbus_message_append_iter_init (rpy, &itr);
        id = xmms_core_get_id ();
        dbus_message_iter_append_uint32 (&itr, id);
 
        dbus_connection_send (conn, rpy, &clientser);
        dbus_message_unref (rpy);
 
        return TRUE;
}


static gboolean
handle_playlist_add (DBusConnection *conn, DBusMessage *msg)
{

        DBusMessageIter itr;
 
        dbus_message_iter_init (msg, &itr);
        if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
                gchar *uri = dbus_message_iter_get_string (&itr);
                XMMS_DBG ("adding to playlist: %s", uri);
                xmms_core_playlist_adduri (uri);
        }
	
	return TRUE;

}

static gboolean
handle_playlist_remove (guint arg)
{
	xmms_core_playlist_remove (arg);

	return TRUE;
}

static gboolean
handle_playlist_list (DBusConnection *conn, DBusMessage *msg)
{
        DBusMessage *rpy;
        DBusMessageIter itr;
        xmms_playlist_t *playlist;
        GList *list,*save;
        int clientser;
                                                                                                    
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

	return TRUE;
}

static gboolean
handle_playlist_shuffle ()
{
	xmms_core_playlist_shuffle ();

	return TRUE;
}

static gboolean
handle_playlist_clear ()
{
	xmms_core_playlist_clear ();
	return TRUE;
}

static gboolean
handle_playlist_move (DBusConnection *conn, DBusMessage *msg)
{
	/** @todo add this function */
	return TRUE;
}

static gboolean
handle_playlist_jump (guint arg)
{
	xmms_core_playlist_jump (arg);
	return TRUE;
}

static gboolean
handle_playlist_mediainfo (DBusConnection *conn, DBusMessage *msg)
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
                dbus_connection_send (conn, reply, &serial);
                dbus_message_unref (reply);
        }

	return TRUE;
}

static gboolean
handle_core_quit ()
{
	xmms_core_quit ();
	return TRUE;
}

static gboolean
handle_core_disconnect (DBusConnection *conn, DBusMessage *msg)
{
	xmms_dbus_connection_t *c;

        g_mutex_lock(connectionslock);
	c = get_conn (conn);
        if (c) {
                connections = g_slist_remove (connections, c);
                g_free (c);
        }
        g_mutex_unlock(connectionslock);
 
        dbus_connection_unref (conn);

	return TRUE;
}

static DBusHandlerResult 
xmms_dbus_callback (DBusMessageHandler *handler, 
	DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	const gchar *msg_name = dbus_message_get_name (msg);
	xmms_dbus_signal_mask_map_t *map;

	XMMS_DBG ("Got msg %s", msg_name);

	map = get_mask_map (msg_name);

	if (map) {

		if (map->callback_with_dbusmsg) {
			map->callback_with_dbusmsg (conn, msg);

		} else if (map->callback_with_intarg) {
			DBusMessageIter itr;
			guint arg;
			
			dbus_message_iter_init (msg, &itr);

			if (dbus_message_iter_get_arg_type (&itr) != DBUS_TYPE_UINT32) {
				XMMS_DBG ("callback_with_intarg set but argument is not UINT32");
			} else {
				arg = dbus_message_iter_get_uint32 (&itr);
				map->callback_with_intarg (arg);
			}
			
		} else if (map->callback_with_noarg) {
			map->callback_with_noarg ();
		}


	} else {
		XMMS_DBG ("No handler for message: %s", msg_name);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}


static void 
new_connect (DBusServer *server, DBusConnection *conn, void * data)
{
	xmms_dbus_connection_t *client;
	int i=0;
	XMMS_DBG ("new connection");

	dbus_connection_ref (conn);

	while (mask_map[i].dbus_name) {
		if (mask_map[i].callback_with_dbusmsg || 
			mask_map[i].callback_with_noarg || 
			mask_map[i].callback_with_intarg) {

			XMMS_DBG ("Register callback for %s", mask_map[i].dbus_name);
			register_handler (conn, xmms_dbus_callback, &mask_map[i].dbus_name, 1);
		}

		i++;
	}

	g_mutex_lock(connectionslock);
	client = g_new0 (xmms_dbus_connection_t, 1);
	client->connection = conn;
	client->signals = 0;
	connections = g_slist_prepend (connections, client);
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
	gint i=0;
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


	while (mask_map[i].dbus_name) {
		if (mask_map[i].object_callback)
			xmms_object_connect (XMMS_OBJECT (core), mask_map[i].dbus_name,
					mask_map[i].object_callback, (gpointer) mask_map[i].dbus_name);
		i++;
	}
	
	XMMS_DBG ("init done!");

        return TRUE;
}
