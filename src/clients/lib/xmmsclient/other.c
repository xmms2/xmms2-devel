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

xmmsc_result_t *
xmmsc_output_mixer_set (xmmsc_connection_t *c, int left, int right)
{
	DBusMessage *msg;
	DBusMessageIter itr;
	xmmsc_result_t *res;

	msg = dbus_message_new_method_call (NULL, 
					    XMMS_OBJECT_OUTPUT, 
					    XMMS_DBUS_INTERFACE, 
					    XMMS_METHOD_MIXERSET);

	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_int32 (&itr, left);
	dbus_message_iter_append_int32 (&itr, right);
	res = xmmsc_send_msg (c, msg);

	return res;
}

xmmsc_result_t *
xmmsc_output_mixer_get (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;

	res = xmmsc_send_msg_no_arg (c, XMMS_OBJECT_OUTPUT, XMMS_METHOD_MIXERGET);
	xmmsc_result_restartable (res, c, XMMS_SIGNAL_OUTPUT_MIXER_CHANGED);

	return res;
}


/**
 * Sets a configvalue in the server.
 */
xmmsc_result_t *
xmmsc_configval_set (xmmsc_connection_t *c, char *key, char *val)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	xmmsc_result_t *res;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_CONFIG, XMMS_DBUS_INTERFACE, XMMS_METHOD_SETVALUE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, key);
	dbus_message_iter_append_string (&itr, val);
	res = xmmsc_send_msg (c, msg);
	dbus_message_unref (msg);

	return res;
}

/**
 * Retrives a list of configvalues in server
 */

xmmsc_result_t *
xmmsc_configval_get (xmmsc_connection_t *c, char *key)
{
	xmmsc_result_t *res;
	DBusMessage *msg;
	DBusMessageIter itr;

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_CONFIG, XMMS_DBUS_INTERFACE, XMMS_METHOD_GET);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, key);
	res = xmmsc_send_msg (c, msg);
	dbus_message_unref (msg);
	
	return res;
}

char *
xmmscs_configval_get (xmmsc_connection_t *c, char *key)
{
	xmmsc_result_t *res;
	char *ret;

	res = xmmsc_configval_get (c, key);
	if (!res)
		return NULL;

	xmmsc_result_wait (res);
	if (!xmmsc_result_get_string (res, &ret)) {
		xmmsc_result_unref (res);
		return NULL;
	}
	xmmsc_result_unref (res);

	return ret;
}

xmmsc_result_t *
xmmsc_configval_list (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_CONFIG, XMMS_METHOD_LIST);
}

x_list_t *
xmmscs_configval_list (xmmsc_connection_t *c)
{
	x_list_t *list;
	xmmsc_result_t *res;

	res = xmmsc_configval_list (c);
	if (!res)
		return NULL;

	xmmsc_result_wait (res);
	if (!xmmsc_result_get_stringlist (res, &list)) {
		xmmsc_result_unref (res);
		return NULL;
	}
	xmmsc_result_unref (res);

	return list;
}

/**
 * Retrives a list of files from url.
 * @todo horrible broken!
 */
/*
void
xmmsc_file_list (xmmsc_connection_t *c, char *url) 
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
*/

/** @} */


