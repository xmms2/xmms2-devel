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
#include "xmms/signal_xmms.h"
#include "xmms/visualisation.h"
#include "xmms/config.h"

#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

/** @defgroup DBus DBus
  * @ingroup XMMSServer
  * @{
  */

typedef struct xmms_dbus_connection_St {
        DBusConnection *connection;
	GList *onchange_list;
	/** Which time did client connect ? */
	guint connecttime;
	/** How many commands have the client issued */
	guint nrcommands;
	/** What is this client ? */
	gchar *clientstr;
} xmms_dbus_connection_t;


typedef struct xmms_dbus_onchange_St {
	xmms_dbus_connection_t *client;
	DBusMessage *msg;
	/** If this bit is set, this should not be used */
	gint gone;
} xmms_dbus_onchange_t;

static DBusServer *server;
static GMutex *connectionslock;
static GMutex *pending_mutex;
static GSList *connections = NULL;
static GHashTable *exported_objects = NULL;
static GHashTable *pending_onchange = NULL;

static void xmms_dbus_handle_arg_value (DBusMessage *msg, xmms_object_method_arg_t *arg);

/*
 * dbus callbacks
 */

GList *
xmms_dbus_stats (GList *list) 
{
	gchar *tmp;
	GSList *n;
	gint i = 1;

	g_mutex_lock (connectionslock);

	for (n = connections; n; n = g_slist_next (n)) {
		xmms_dbus_connection_t *c = n->data;
		tmp = g_strdup_printf ("dbus.client%d.name=%s", i, c->clientstr);
		list = g_list_append (list, tmp);
		tmp = g_strdup_printf ("dbus.client%d.commands=%u", i, c->nrcommands);
		list = g_list_append (list, tmp);
		tmp = g_strdup_printf ("dbus.client%d.uptime=%u", i, xmms_util_time ()-c->connecttime);
		list = g_list_append (list, tmp);
		i++;
	}

	g_mutex_unlock (connectionslock);

	return list;
}

void
xmms_dbus_register_object (const gchar *objectpath, xmms_object_t *object)
{
	gchar *fullpath;

	fullpath = g_strdup_printf ("/xmms/%s", objectpath);

	XMMS_DBG ("REGISTERING: '%s'", fullpath);

	if (!exported_objects)
		exported_objects =
			g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
			                       (GDestroyNotify) __int_xmms_object_unref);

	xmms_object_ref (object);
	g_hash_table_insert (exported_objects, fullpath, object);

}

void
xmms_dbus_unregister_object (const gchar *objectpath)
{
	gchar *fullpath;

	fullpath = g_strdup_printf ("/xmms/%s", objectpath);

	XMMS_DBG ("UNREGISTERING: '%s'", fullpath);

	g_hash_table_remove (exported_objects, fullpath);
	g_free (fullpath);
}

static void
xmms_dbus_onchange (xmms_object_t *object, gconstpointer arg, gpointer userdata)
{
	GList *l;
	DBusMessage *retmsg;
	gchar *signal = userdata;

	if (pending_onchange) {
		g_mutex_lock (pending_mutex);
		l = g_hash_table_lookup (pending_onchange, signal);
		if (l) {
			GList *n;
			for (n = l; n; n = g_list_next (n)) {
				gint serial;
				xmms_dbus_onchange_t *oc = n->data;

				if (!oc->gone) {
					retmsg = dbus_message_new_method_return (oc->msg);
					xmms_dbus_handle_arg_value (retmsg, (xmms_object_method_arg_t*) arg);

					dbus_connection_send (oc->client->connection, retmsg, &serial);
					dbus_message_unref (retmsg);

					dbus_message_unref (oc->msg);
					oc->client->onchange_list = g_list_remove (oc->client->onchange_list, oc);
				}
				g_free (oc);

			}

			g_list_free (l);

			g_hash_table_remove (pending_onchange, signal);
		}
		g_mutex_unlock (pending_mutex);
	}
				   
}

void
xmms_dbus_register_onchange (xmms_object_t *object, gchar *signal)
{
	XMMS_DBG ("Adding On Change signal: '%s'", signal);

	xmms_object_connect (XMMS_OBJECT (object), signal, xmms_dbus_onchange, signal);
}

static DBusHandlerResult
xmms_dbus_clientcall (DBusConnection *conn, DBusMessage *msg, void *userdata)
{
	xmms_dbus_connection_t *client = userdata;
	xmms_dbus_onchange_t *oc;
	DBusMessageIter itr;

	g_return_val_if_fail (client, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
	g_return_val_if_fail (client->connection == conn, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcmp (dbus_message_get_member (msg), XMMS_METHOD_ONCHANGE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
		GList *list;
		gchar *signal = dbus_message_iter_get_string (&itr);
		gchar *t = g_strdup (signal);


		g_mutex_lock (pending_mutex);
		if (!pending_onchange)
			pending_onchange = g_hash_table_new (g_str_hash, g_str_equal);

		oc = g_new0 (xmms_dbus_onchange_t, 1);
		oc->msg = msg;
		oc->client = client;
		dbus_message_ref (msg);

		list = g_hash_table_lookup (pending_onchange, t);

		if (list) {
			g_hash_table_remove (pending_onchange, t);
		}

		list = g_list_append (list, oc);
		client->onchange_list = g_list_append (client->onchange_list, oc);

		g_hash_table_insert (pending_onchange, t, list);
		
		g_mutex_unlock (pending_mutex);

		if (!strcmp (signal, XMMS_SIGNAL_VISUALISATION_SPECTRUM)) {
			xmms_visualisation_users_inc ();
		}

		g_free (signal);
	}

	return DBUS_HANDLER_RESULT_HANDLED;

}

static void
hash_to_dict (gpointer key, gpointer value, gpointer udata)
{
        gchar *k = key;
        gchar *v = value;
        DBusMessageIter *itr = udata;

	if (v) {
		dbus_message_iter_append_dict_key (itr, k);
		dbus_message_iter_append_string (itr, v);
	}
	
}

static void
xmms_dbus_handle_playlist_chmsg (DBusMessageIter *itr, xmms_playlist_changed_msg_t *chpl)
{
	dbus_message_iter_append_uint32 (itr, chpl->type);
	dbus_message_iter_append_uint32 (itr, chpl->id);
	dbus_message_iter_append_uint32 (itr, chpl->arg);
}

static void
xmms_dbus_do_hashtable (DBusMessageIter *itr, GHashTable *table)
{
	DBusMessageIter dictitr;

	dbus_message_iter_append_dict (itr, &dictitr);
	g_hash_table_foreach (table, hash_to_dict, &dictitr);
}

static void
xmms_dbus_do_playlist_entry (DBusMessageIter *itr, xmms_playlist_entry_t *entry)
{
	const gchar *url;
	DBusMessageIter dictitr;

	url = xmms_playlist_entry_url_get (entry);

	dbus_message_iter_append_dict (itr, &dictitr);

	/* add id to Dict */
	dbus_message_iter_append_dict_key (&dictitr, "id");
	dbus_message_iter_append_uint32 (&dictitr, xmms_playlist_entry_id_get (entry));

	/* add url to Dict */
	if (url) {
		dbus_message_iter_append_dict_key (&dictitr, "url");
		dbus_message_iter_append_string (&dictitr, url);
	}

	/* add the rest of the properties to Dict */
	xmms_playlist_entry_property_foreach (entry, hash_to_dict, &dictitr);
}

static void
xmms_dbus_handle_arg_value (DBusMessage *msg, xmms_object_method_arg_t *arg)
{
	DBusMessageIter itr;

	
	dbus_message_iter_init (msg, &itr);

	switch (arg->rettype) {
		case XMMS_OBJECT_METHOD_ARG_STRING:
			dbus_message_iter_append_string (&itr, arg->retval.string); /*convert to utf8?*/
			break;
		case XMMS_OBJECT_METHOD_ARG_UINT32:
			dbus_message_iter_append_uint32 (&itr, arg->retval.uint32);
			break;
		case XMMS_OBJECT_METHOD_ARG_INT32:
			dbus_message_iter_append_int32 (&itr, arg->retval.int32);
			break;
		case XMMS_OBJECT_METHOD_ARG_STRINGLIST:
			{
				GList *l = arg->retval.stringlist;

				while (l) {
					dbus_message_iter_append_string (&itr, l->data);
					l = g_list_delete_link (l, l);
				}
				break;
			}
		case XMMS_OBJECT_METHOD_ARG_UINTLIST:
			{
				GList *l = arg->retval.uintlist;

				while (l) {
					dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_UINT (l->data));
					l = g_list_delete_link (l, l);
				}
				break;
			}
		case XMMS_OBJECT_METHOD_ARG_INTLIST:
			{
				GList *l = arg->retval.intlist;

				while (l) {
					dbus_message_iter_append_int32 (&itr, GPOINTER_TO_INT (l->data));
					l = g_list_delete_link (l, l);
				}
				break;
			}
		case XMMS_OBJECT_METHOD_ARG_HASHLIST:
			{
				GList *l = arg->retval.hashlist;

				while (l) {
					if (l->data) {
						xmms_dbus_do_hashtable (&itr, (GHashTable *)l->data);
						g_hash_table_destroy (l->data);
					}
					l = g_list_delete_link (l,l);
				}
				break;
			}
		case XMMS_OBJECT_METHOD_ARG_PLCH:
			{
				xmms_playlist_changed_msg_t *chmsg = arg->retval.plch;
				xmms_dbus_handle_playlist_chmsg (&itr, chmsg);
			}
			break;
		case XMMS_OBJECT_METHOD_ARG_PLAYLIST_ENTRY: 
			{
				if (arg->retval.playlist_entry)  {
					xmms_dbus_do_playlist_entry (&itr, arg->retval.playlist_entry);
					xmms_object_unref (arg->retval.playlist_entry);
				}

				break;
			}
		case XMMS_OBJECT_METHOD_ARG_NONE:
			break;
		default:
			xmms_log_error ("Unknown returnvalue: %d, couldn't serialize message", arg->rettype);
			break;
	}
}

static DBusHandlerResult
xmms_dbus_methodcall (DBusConnection *conn, DBusMessage *msg, void *userdata)
{
	xmms_dbus_connection_t *client = userdata;
	xmms_object_method_arg_t arg;
	gint args = 0;
	DBusMessageIter iter;
	xmms_object_t *obj;
	DBusMessage *retmsg;
	int serial;
	gint i;


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
	xmms_error_reset (&arg.error);

	dbus_message_iter_init (msg, &iter);

	for (;;) {
		gint type;
		
		type = dbus_message_iter_get_arg_type (&iter);
		
		if (type == DBUS_TYPE_INVALID)
			break;
		
		if (args + 1 > XMMS_OBJECT_METHOD_MAX_ARGS) {
			xmms_log_error ("Too many arguments...");
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
	
	if (xmms_error_isok (&arg.error)) {
		retmsg = dbus_message_new_method_return (msg);
		xmms_dbus_handle_arg_value (retmsg, &arg);
	} else {
		/* create error message */
		retmsg = dbus_message_new_error (msg, xmms_error_type_get_str (&arg.error), xmms_error_message_get (&arg.error));
	}

	for (i = 0; i < args; i ++) {
		if (arg.types[i] == XMMS_OBJECT_METHOD_ARG_STRING) {
			g_free (arg.values[i].string);
		}
	}


	dbus_connection_send (conn, retmsg, &serial);
	dbus_message_unref (retmsg);
	client->nrcommands++;

	return DBUS_HANDLER_RESULT_HANDLED;

}

static DBusHandlerResult
xmms_dbus_localcall (DBusConnection *conn, DBusMessage *msg, void *userdata)
{
	GList *n = NULL;
	xmms_dbus_connection_t *client = userdata;

	g_return_val_if_fail (client, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);
	g_return_val_if_fail (client->connection == conn, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (!strcmp (dbus_message_get_member (msg), "Disconnected")) {
		XMMS_DBG ("Client disconnected");

		g_mutex_lock(connectionslock);
		connections = g_slist_remove (connections, client);
		g_mutex_unlock(connectionslock);

		g_mutex_lock (pending_mutex);
		for (n = client->onchange_list; n; n = g_list_next (n)) {
			((xmms_dbus_onchange_t*)n->data)->gone = 1;
		}
		g_mutex_unlock (pending_mutex);

		g_list_free (client->onchange_list);
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

	XMMS_DBG ("new connection");

	memset (&vtable, 0, sizeof (vtable));

	dbus_connection_ref (conn);

	client = g_new0 (xmms_dbus_connection_t, 1);
	client->connection = conn;
	client->onchange_list = NULL;
	client->connecttime = xmms_util_time ();
	client->nrcommands = 0;

	vtable.message_function = xmms_dbus_clientcall;
	dbus_connection_register_fallback (conn, "/xmms/client",
	                                   &vtable, client);

	vtable.message_function = xmms_dbus_methodcall;
	dbus_connection_register_fallback (conn, "/xmms",
	                                   &vtable, client);

	vtable.message_function = xmms_dbus_localcall;
	dbus_connection_register_fallback (conn, "/org/freedesktop/Local",
	                                   &vtable, client);

	g_mutex_lock(connectionslock);
	connections = g_slist_prepend (connections, client);
	g_mutex_unlock(connectionslock);

	dbus_connection_setup_with_g_main (conn, g_main_context_default());

	/** 
	 * @todo How does D-BUS properties work? We'll need them for
	 * clientstring and protocol version.
	 */
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
        DBusError err;
	DBusConnection *conn;

	dbus_g_thread_init (); /* dbus_enable_deadlocks (); */

	connectionslock = g_mutex_new ();
	pending_mutex = g_mutex_new ();

	if (!exported_objects)
		exported_objects =
			g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
			                       (GDestroyNotify) __int_xmms_object_unref);


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
                xmms_log_fatal ("couldn't create server!\n");
                return FALSE;
        }
        dbus_server_ref (server);
	dbus_server_set_new_connection_function (server, new_connect,
						 NULL, NULL);
	
	dbus_server_setup_with_g_main (server, g_main_context_default());

	XMMS_DBG ("init done!");

        return TRUE;
}

void
xmms_dbus_shutdown ()
{
	if (exported_objects) {
		g_hash_table_destroy (exported_objects);
		exported_objects = NULL;
	}

	if (server) {
		dbus_server_disconnect (server);
		dbus_server_unref (server);
		server = NULL;
	}
}
/** @} */
