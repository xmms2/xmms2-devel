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




/**
 * @defgroup OtherControl OtherControl
 * @ingroup XMMSClient
 * @brief This controls various other functions of the XMMS server.
 *
 * @{
 */


/**
 * Sets a configvalue in the server.
 */
void
xmmsc_configval_set (xmmsc_connection_t *c, gchar *key, gchar *val)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_CONFIG, XMMS_DBUS_INTERFACE, XMMS_METHOD_SETVALUE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, key);
	dbus_message_iter_append_string (&itr, val);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Retrives a list of files from url.
 */
void
xmmsc_file_list (xmmsc_connection_t *c, gchar *url) 
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_TRANSPORT, XMMS_DBUS_INTERFACE, XMMS_METHOD_LIST);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, url);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/** @} */


