/** @file
 * D-BUS handler.
 *
 * This code is responsible of setting up a D-BUS server that clients
 * can connect to. It serializes xmms-signals into dbus-messages that
 * are passed to the clients. The clients can send dbus-messages that
 * causes xmms-signals to be emitted.
 *
 * @sa http://www.freedesktop.org/software/dbus/
 *
 */

/* YES! I know that this api may change under my feet */
#define DBUS_API_SUBJECT_TO_CHANGE

#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/core.h"
#include "xmms/signal_xmms.h"
#include "xmms/visualisation.h"
#include "xmms/config.h"

#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

static DBusServer *server;
static GMutex *connectionslock;
static GSList *connections = NULL;
static GHashTable *exported_objects = NULL;

extern xmms_core_t *core;

/** Ids in signal mask */
typedef enum {
	XMMS_SIGNAL_MASK_PLAYBACK_CURRENTID = 1 << 0,
	XMMS_SIGNAL_MASK_PLAYBACK_PLAYTIME = 1 << 1,
	XMMS_SIGNAL_MASK_PLAYBACK_STOP = 1 << 2,
	XMMS_SIGNAL_MASK_PLAYLIST_MEDIAINFO_ID = 1 << 3,
	XMMS_SIGNAL_MASK_PLAYLIST_CHANGED = 1 << 4,
	XMMS_SIGNAL_MASK_CORE_INFORMATION = 1 << 5,
	XMMS_SIGNAL_MASK_VISUALISATION_SPECTRUM = 1 << 6,
	XMMS_SIGNAL_MASK_TRANSPORT_LIST = 1 << 7,
} xmms_dbus_signal_mask_t;


static void send_playback_stop (xmms_object_t *object, 
	gconstpointer data, gpointer userdata);
static void send_playlist_mediainfo_id (xmms_object_t *object, 
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

static void broadcast_msg (DBusMessage *msg, const char *type);


static void register_visualisation_spectrum (gboolean state);

typedef void (*xmms_dbus_object_callback_t) (xmms_object_t *object, gconstpointer data, gpointer userdata);

typedef void (*xmms_dbus_callback_on_register_t) (gboolean state);

typedef struct xmms_dbus_signal_mask_map_St {
	/** The dbus signalname, ie org.xmms.playback.play */
	gchar *dbus_name;
	xmms_dbus_signal_mask_t mask;

	/** This will be called when the xmms_name 
	  is received from the core object */
	xmms_dbus_object_callback_t object_callback;

	xmms_dbus_callback_on_register_t register_callback;

} xmms_dbus_signal_mask_map_t;

/** @todo this should be a hashtable maybe. should be faster. */
static xmms_dbus_signal_mask_map_t mask_map [] = {

	{ XMMS_SIGNAL_PLAYBACK_CURRENTID,
		XMMS_SIGNAL_MASK_PLAYBACK_CURRENTID, 
		send_playback_currentid, NULL },

	{ XMMS_SIGNAL_PLAYBACK_PLAYTIME,
		XMMS_SIGNAL_MASK_PLAYBACK_PLAYTIME, 
		send_playback_playtime, NULL },

	{ XMMS_SIGNAL_PLAYBACK_STOP,
		XMMS_SIGNAL_MASK_PLAYBACK_STOP, 
		send_playback_stop, NULL },

	{ XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID, 
		XMMS_SIGNAL_MASK_PLAYLIST_MEDIAINFO_ID, 
		send_playlist_mediainfo_id, NULL },
/*	{ XMMS_SIGNAL_PLAYLIST_CHANGED,
		XMMS_SIGNAL_MASK_PLAYLIST_CHANGED, 
		send_playlist_changed, NULL, NULL, NULL },*/
	{ XMMS_SIGNAL_CORE_INFORMATION, 
		XMMS_SIGNAL_MASK_CORE_INFORMATION, 
		send_core_information, NULL },


	{ XMMS_SIGNAL_VISUALISATION_SPECTRUM,
		XMMS_SIGNAL_MASK_VISUALISATION_SPECTRUM, 
		send_visualisation_spectrum, 
		register_visualisation_spectrum},

/*
	{ XMMS_SIGNAL_TRANSPORT_LIST, 
	  	XMMS_SIGNAL_MASK_TRANSPORT_LIST,
		NULL, handle_transport_list, NULL, NULL },
*/

	{ NULL, 0, NULL, NULL },
};

typedef struct xmms_dbus_connection_St {
        gint signals;
        DBusConnection *connection;
} xmms_dbus_connection_t;


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


/*
 * object callbacks
 */

#if 0 /* @todo */

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
                        msg = dbus_message_new_signal (XMMS_OBJECT_PLAYLIST, NULL, XMMS_METHOD_ADD);
                        dbus_message_append_iter_init (msg, &itr);
                        dbus_message_iter_append_uint32 (&itr, chmsg->id);
                        dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_INT (chmsg->arg));
                        break;
                case XMMS_PLAYLIST_CHANGED_SHUFFLE:
                        msg = dbus_message_new_signal (XMMS_OBJECT_PLAYLIST, NULL, XMMS_METHOD_SHUFFLE);
                        break;
                case XMMS_PLAYLIST_CHANGED_SORT:
                        msg = dbus_message_new_signal (XMMS_OBJECT_PLAYLIST, NULL, XMMS_METHOD_SORT);
                        break;
                case XMMS_PLAYLIST_CHANGED_CLEAR:
                        msg = dbus_message_new_signal (XMMS_OBJECT_PLAYLIST, NULL, XMMS_METHOD_CLEAR);
                        break;
                case XMMS_PLAYLIST_CHANGED_REMOVE:
                        msg = dbus_message_new_signal (XMMS_OBJECT_PLAYLIST, NULL, XMMS_METHOD_REMOVE);
                        dbus_message_append_iter_init (msg, &itr);
                        dbus_message_iter_append_uint32 (&itr, chmsg->id);
                        break;
                case XMMS_PLAYLIST_CHANGED_SET_POS:
                        msg = dbus_message_new_signal (XMMS_OBJECT_PLAYLIST, NULL, XMMS_METHOD_JUMP);
                        dbus_message_append_iter_init (msg, &itr);
                        dbus_message_iter_append_uint32 (&itr, chmsg->id);
                        break;
                case XMMS_PLAYLIST_CHANGED_MOVE:
                        msg = dbus_message_new_signal (XMMS_OBJECT_PLAYLIST, NULL, XMMS_METHOD_MOVE);
                        dbus_message_append_iter_init (msg, &itr);
                        dbus_message_iter_append_uint32 (&itr, chmsg->id);
                        dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_INT (chmsg->arg));
                        break;
        }

/*
        XMMS_DBG ("Sending playlist changed message: %s", dbus_message_get_name (msg));
*/

        g_mutex_lock (connectionslock);

	/** @todo broadcast_msg (...); */

        g_mutex_unlock (connectionslock);

        dbus_message_unref (msg);
	
}

#endif

static void
send_playback_stop (xmms_object_t *object,
		gconstpointer data,
		gpointer userdata)
{
        g_mutex_lock (connectionslock);

        if (connections) {
                DBusMessage *msg;

                msg = dbus_message_new_signal (XMMS_OBJECT_PLAYBACK, XMMS_DBUS_INTERFACE, XMMS_METHOD_STOP);
		broadcast_msg (msg, XMMS_SIGNAL_PLAYBACK_STOP);
                dbus_message_unref (msg);
        }

        g_mutex_unlock (connectionslock);
}

static void
send_playlist_mediainfo_id (xmms_object_t *object,
			    gconstpointer data,
		            gpointer userdata)
{
        g_mutex_lock (connectionslock);

        if (connections) {
                DBusMessage *msg;
                DBusMessageIter itr;
		guint id = GPOINTER_TO_UINT (data);

		XMMS_DBG ("Sending mediainfo_id for id %d", id);
		
                msg = dbus_message_new_signal (XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_MEDIAINFO_ID);
                dbus_message_append_iter_init (msg, &itr);
                dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_UINT(data));
		broadcast_msg (msg, XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID);
                dbus_message_unref (msg);
        }

        g_mutex_unlock (connectionslock);
}

static void
broadcast_msg (DBusMessage *msg, const char *type)
{
	xmms_dbus_signal_mask_map_t *map = get_mask_map (type);
	GSList *item;
	xmms_dbus_connection_t *con;
	int clientser;

	for (item = connections; item; item = g_slist_next (item)) {
		con = (xmms_dbus_connection_t *)item->data;
		if (con->signals & map->mask) {
			dbus_connection_send (con->connection, msg, &clientser);
		}
	}
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

                msg = dbus_message_new_signal (XMMS_OBJECT_PLAYBACK, XMMS_DBUS_INTERFACE, XMMS_METHOD_PLAYTIME);
                dbus_message_append_iter_init (msg, &itr);
                dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_UINT(data));

		broadcast_msg (msg, XMMS_SIGNAL_PLAYBACK_PLAYTIME);
                dbus_message_unref (msg);
        }

	g_mutex_unlock(connectionslock);

}


static void
register_visualisation_spectrum (gboolean state)
{
	if (state) {
		XMMS_DBG ("someone wants vis-data!");
		xmms_visualisation_users_inc ();
	} else {
		xmms_visualisation_users_dec ();
		XMMS_DBG ("someone doesn't want vis-data anymore!");
	}
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

                msg = dbus_message_new_signal (XMMS_OBJECT_VISUALISATION, XMMS_DBUS_INTERFACE, XMMS_METHOD_SPECTRUM);
                dbus_message_append_iter_init (msg, &itr);
		dbus_message_iter_append_double_array (&itr, val, FFT_LEN/2+1);
		broadcast_msg (msg, XMMS_SIGNAL_VISUALISATION_SPECTRUM);
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

                msg = dbus_message_new_signal (XMMS_OBJECT_PLAYBACK, XMMS_DBUS_INTERFACE, XMMS_METHOD_CURRENTID);
                dbus_message_append_iter_init (msg, &itr);
                dbus_message_iter_append_uint32 (&itr, xmms_core_get_id ((xmms_core_t *)object));
		broadcast_msg (msg, XMMS_SIGNAL_PLAYBACK_CURRENTID);
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
 
                msg = dbus_message_new_signal (XMMS_OBJECT_CORE, NULL, XMMS_METHOD_INFORMATION);
                dbus_message_append_iter_init (msg, &itr);
                dbus_message_iter_append_string (&itr, (gchar *)data);
		broadcast_msg (msg, XMMS_SIGNAL_CORE_INFORMATION);
                dbus_message_unref (msg);
        }

        g_mutex_unlock (connectionslock);
}

/*
 * dbus callbacks
 */

#if 0 /** @todo THESE ARE NOT YET CONVERTET */

static gboolean
handle_playlist_save (DBusConnection *conn, DBusMessage *msg)
{
        DBusMessageIter itr;
 
        dbus_message_iter_init (msg, &itr);
        if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
                gchar *filename = dbus_message_iter_get_string (&itr);
                xmms_core_playlist_save (filename);
		g_free (filename);
        }
	
	return TRUE;
}

static gboolean
handle_playlist_mode_set (guint arg)
{
	xmms_core_playlist_mode_set ((xmms_playlist_mode_t) arg);
	return TRUE;
}


static gboolean
handle_transport_list (DBusConnection *conn, DBusMessage *msg)
{
        DBusMessage *reply;
        DBusMessageIter itr;
	gchar *path = NULL;
 
        dbus_message_iter_init (msg, &itr);
        if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
                path = dbus_message_iter_get_string (&itr);
        }

	if (path) {
		GList *paths, *tmp;
		gint clientser;

		paths = xmms_transport_list (path);
		
		reply = dbus_message_new_signal (XMMS_OBJECT_TRANSPORT, NULL, XMMS_METHOD_LIST);
		dbus_message_append_iter_init (reply, &itr);

		for (tmp = paths; tmp; tmp = g_list_next (tmp)) {
			xmms_transport_entry_t *e = tmp->data;
			dbus_message_iter_append_string (&itr, xmms_transport_entry_path_get (e));
			if (xmms_transport_entry_type_get (e) == XMMS_TRANSPORT_ENTRY_DIR)
				dbus_message_iter_append_boolean (&itr, FALSE);
			else
				dbus_message_iter_append_boolean (&itr, TRUE);
		}
	
		dbus_connection_send (conn, reply, &clientser);
		dbus_message_unref (reply);

		if (paths)
			xmms_transport_list_free (paths);
	}

	return TRUE;
}

#endif

void
xmms_dbus_register_object (const gchar *objectpath, xmms_object_t *object)
{
	gchar *fullpath;

	fullpath = g_strdup_printf ("/xmms/%s", objectpath);

	XMMS_DBG ("REGISTERING: '%s'", fullpath);

	if (!exported_objects)
		exported_objects = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_insert (exported_objects, fullpath, object);

}


static DBusHandlerResult
xmms_dbus_clientcall (DBusConnection *conn, DBusMessage *msg, void *userdata)
{
	xmms_dbus_connection_t *client = userdata;
	DBusMessageIter itr;

	g_return_val_if_fail (client, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
	g_return_val_if_fail (client->connection == conn, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcmp (dbus_message_get_member (msg), XMMS_METHOD_REGISTER) &&
	    strcmp (dbus_message_get_member (msg), XMMS_METHOD_UNREGISTER))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
		gchar *signal = dbus_message_iter_get_string (&itr);
		xmms_dbus_signal_mask_map_t *map = get_mask_map (signal);
		XMMS_DBG ("request for signal %s", signal);
		if (map) {
			XMMS_DBG ("with map: %d", map->mask);
			g_mutex_lock(connectionslock);

			if (!(client->signals & map->mask) && map->register_callback) {
				map->register_callback (!strcmp (dbus_message_get_member (msg), XMMS_METHOD_REGISTER));
			}
			if (!strcmp (dbus_message_get_member (msg), XMMS_METHOD_REGISTER)) {
				client->signals |= map->mask;
			} else {
				client->signals &= ~map->mask;
			}
			
			g_mutex_unlock(connectionslock);
		}
		g_free (signal);
	}

	return DBUS_HANDLER_RESULT_HANDLED;

}

static DBusHandlerResult
xmms_dbus_methodcall (DBusConnection *conn, DBusMessage *msg, void *userdata)
{
	xmms_dbus_connection_t *client = userdata;
	xmms_object_method_arg_t arg;
	gint args = 0;
	gint len;
	DBusMessageIter iter;
	xmms_object_t *obj;

	g_return_val_if_fail (client, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
	g_return_val_if_fail (client->connection == conn, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	XMMS_DBG ("Methodcall %s on %s", dbus_message_get_member (msg), dbus_message_get_path (msg));

	obj = g_hash_table_lookup (exported_objects, dbus_message_get_path (msg));
	
	if (!obj) {
		XMMS_DBG ("Unknown object: %s", dbus_message_get_path (msg));
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	
	/* */

	memset (&arg, 0, sizeof (arg));

	dbus_message_iter_init (msg, &iter);

	for (;;) {
		gint type;
		
		type = dbus_message_iter_get_arg_type (&iter);
		
		if (type == DBUS_TYPE_INVALID)
			break;
		
		if (args + 1 > XMMS_OBJECT_METHOD_MAX_ARGS) {
			XMMS_DBG ("Too many arguments...");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		
		switch (type) {
		case DBUS_TYPE_INT32:
			arg.values[args].int32 = dbus_message_iter_get_int32 (&iter);
			arg.types[args] = XMMS_OBJECT_METHOD_ARG_INT32;
			break;
		case DBUS_TYPE_UINT32:
			arg.values[args].uint32 = dbus_message_iter_get_uint32 (&iter);
			arg.types[args] = XMMS_OBJECT_METHOD_ARG_UINT32;
			break;
		case DBUS_TYPE_STRING:
			arg.values[args].string = dbus_message_iter_get_string (&iter);
			arg.types[args] = XMMS_OBJECT_METHOD_ARG_STRING;
			break;
		default:
			XMMS_DBG ("Unknown argument: %d - couldn't deserialize message", type);
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		args++;
		if (!dbus_message_iter_next (&iter))
			break;
	}
	
	xmms_object_method_call (obj, dbus_message_get_member (msg), &arg);
	
	if (arg.rettype != XMMS_OBJECT_METHOD_ARG_NONE) {
		gchar *url;
		DBusMessage *retmsg;
		int serial;
		DBusMessageIter itr;
		DBusMessageIter dictitr;

		retmsg = dbus_message_new_method_return (msg);
		dbus_message_append_iter_init (retmsg, &itr);

		switch (arg.rettype) {
		case XMMS_OBJECT_METHOD_ARG_UINT32:

			dbus_message_iter_append_uint32 (&itr, arg.retval.uint32);
			break;
		case XMMS_OBJECT_METHOD_ARG_PLAYLIST:

			len = g_list_length (arg.retval.playlist);

			dbus_message_iter_append_uint32 (&itr, len);

			if (len > 0) {
				GList *list = arg.retval.playlist;
				guint32 *arr;
				int i = 0;

				arr = g_new0 (guint32, len);
				while (list) {
					xmms_playlist_entry_t *entry=list->data;
					arr[i++] = xmms_playlist_entry_id_get (entry);
					xmms_playlist_entry_unref (entry);
					list = g_list_next (list);
				}
				
				dbus_message_iter_append_uint32_array (&itr, arr, len);
				g_free (arr);
			}

			g_list_free (arg.retval.playlist);

			break;
		case XMMS_OBJECT_METHOD_ARG_PLAYLIST_ENTRY:
			url = xmms_playlist_entry_url_get (arg.retval.playlist_entry);

			dbus_message_iter_append_dict (&itr, &dictitr);
			
			/* add id to Dict */
			dbus_message_iter_append_dict_key (&dictitr, "id");
			dbus_message_iter_append_uint32 (&dictitr, xmms_playlist_entry_id_get (arg.retval.playlist_entry));
			
			/* add url to Dict */
			if (url) {
				dbus_message_iter_append_dict_key (&dictitr, "url");
				dbus_message_iter_append_string (&dictitr, url);
			}
			
			/* add the rest of the properties to Dict */
			xmms_playlist_entry_property_foreach (arg.retval.playlist_entry, hash_to_dict, &dictitr);
			
			xmms_playlist_entry_unref (arg.retval.playlist_entry);
			
			break;
		default:
			XMMS_DBG ("Unknown returnvalue: %d, couldn't serialize message", arg.rettype);
			break;
		}

		dbus_connection_send (conn, retmsg, &serial);
                dbus_message_unref (retmsg);
	}

	return DBUS_HANDLER_RESULT_HANDLED;

}

static DBusHandlerResult
xmms_dbus_localcall (DBusConnection *conn, DBusMessage *msg, void *userdata)
{
	xmms_dbus_connection_t *client = userdata;

	g_return_val_if_fail (client, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
	g_return_val_if_fail (client->connection == conn, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (!strcmp (dbus_message_get_member (msg), "Disconnected")) {
		XMMS_DBG ("Client disconnected");

		g_mutex_lock(connectionslock);
		connections = g_slist_remove (connections, client);
		g_mutex_unlock(connectionslock);
		g_free (client);
		dbus_connection_unref (conn);
		
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	XMMS_DBG ("Unknown localcall: %s on %s", dbus_message_get_member (msg), dbus_message_get_path (msg));

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

}


static void 
new_connect (DBusServer *server, DBusConnection *conn, void * data)
{
	xmms_dbus_connection_t *client;
	DBusObjectPathVTable vtable;
	static const char *xmms_obj[] = {"xmms", NULL};
	static const char *client_obj[] = {"xmms", "client", NULL};
	static const char *local_obj[] = {"org", "freedesktop", "Local", NULL};

	XMMS_DBG ("new connection");

	memset (&vtable, 0, sizeof (vtable));

	dbus_connection_ref (conn);

	client = g_new0 (xmms_dbus_connection_t, 1);
	client->connection = conn;
	client->signals = 0;

	vtable.message_function = xmms_dbus_clientcall;
	dbus_connection_register_fallback (conn, client_obj, &vtable, client);

	vtable.message_function = xmms_dbus_methodcall;
	dbus_connection_register_fallback (conn, xmms_obj, &vtable, client);

	vtable.message_function = xmms_dbus_localcall;
	dbus_connection_register_fallback (conn, local_obj, &vtable, client);

	g_mutex_lock(connectionslock);
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
xmms_dbus_init (const gchar *path) 
{
	gint i=0;
        DBusError err;
	DBusConnection *conn;

	dbus_g_thread_init (); /* dbus_enable_deadlocks (); */

	connectionslock = g_mutex_new ();

	if (!exported_objects)
		exported_objects = g_hash_table_new (g_str_hash, g_str_equal);

        dbus_error_init (&err);

	/* Check if other server is running */
	if ((conn = dbus_connection_open (path, &err))) {
		dbus_connection_disconnect (conn);
		dbus_connection_unref (conn);
		xmms_log_fatal ("Socket %s already in use...", path);
	}

	if (conn)
		dbus_connection_unref (conn);

	dbus_error_free (&err);
        dbus_error_init (&err);

        server = dbus_server_listen (path, &err);
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
