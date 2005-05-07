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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"

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
xmmsc_result_t *
xmmsc_configval_set (xmmsc_connection_t *c, char *key, char *val)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_SETVALUE);
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_string (msg, val);
	res = xmmsc_send_msg (c, msg);

	return res;
}

/**
 * Retrives a list of configvalues in server
 */

xmmsc_result_t *
xmmsc_configval_get (xmmsc_connection_t *c, char *key)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_GETVALUE);
	xmms_ipc_msg_put_string (msg, key);
	res = xmmsc_send_msg (c, msg);
	
	return res;
}

char *
xmmscs_configval_get (xmmsc_connection_t *c, char *key)
{
	xmmsc_result_t *res;
	char *ret;
	char *str;

	res = xmmsc_configval_get (c, key);
	if (!res)
		return NULL;

	xmmsc_result_wait (res);
	if (!xmmsc_result_get_string (res, &str)) {
		xmmsc_result_unref (res);
		return NULL;
	}
	ret = strdup (str);
	xmmsc_result_unref (res);

	return ret;
}

xmmsc_result_t *
xmmsc_configval_list (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_LISTVALUES);
}

x_list_t *
xmmscs_configval_list (xmmsc_connection_t *c)
{
	x_list_t *list, *l, *ret = NULL;
	xmmsc_result_t *res;

	res = xmmsc_configval_list (c);
	if (!res)
		return NULL;

	xmmsc_result_wait (res);
	if (!xmmsc_result_get_stringlist (res, &list)) {
		xmmsc_result_unref (res);
		return NULL;
	}

	for (l = list; l; l = x_list_next (l)) {
		ret = x_list_append (ret, strdup ((char *)l->data));
	}
	
	xmmsc_result_unref (res);

	return ret;
}

xmmsc_result_t *
xmmsc_broadcast_configval_changed (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED);
}


xmmsc_result_t *
xmmsc_signal_visualisation_data (xmmsc_connection_t *c)
{
	return xmmsc_send_signal_msg (c, XMMS_IPC_SIGNAL_VISUALISATION_DATA);
}

/** @} */


