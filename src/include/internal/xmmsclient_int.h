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




#ifndef __XMMSCLIENT_INT_H__
#define __XMMSCLIENT_INT_H__

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>

#include "xmms/xmmswatch.h"

#include "internal/xhash-int.h"
#include "internal/xlist-int.h"

/**
 * @typedef xmmsc_connection_t
 *
 * Holds all data about the current connection to
 * the XMMS server.
 */

struct xmmsc_connection_St {
	DBusConnection *conn;	
	x_hash_t *callbacks;
	x_hash_t *replies;
	char *error;
	int timeout;
	xmmsc_watch_callback_t watch_callback;
	void *data;
};


xmmsc_result_t * xmmsc_send_msg_no_arg (xmmsc_connection_t *c, char *object, char *method);
xmmsc_result_t * xmmsc_send_msg (xmmsc_connection_t *c, DBusMessage *msg);
xmmsc_result_t * xmmsc_send_on_change (xmmsc_connection_t *c, DBusMessage *msg);
void xmmsc_connection_add_reply (xmmsc_connection_t *c, int serial, char *type);
x_hash_t *xmmsc_deserialize_mediainfo (DBusMessageIter *itr);
xmmsc_result_t *xmmsc_result_new (DBusPendingCall *pending); 

#endif

