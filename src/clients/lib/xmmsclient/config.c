/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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
#include <sys/types.h>
#include <unistd.h>


#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"

/**
 * @defgroup ConfigControl ConfigControl
 * @ingroup XMMSClient
 * @brief This controls configuration values on the XMMS server.
 *
 * @{
 */

/**
 * Registers a configvalue in the server.
 *
 * @param c The connection structure.
 * @param key should be &lt;clientname&gt;.myval like cli.path or something like that.
 * @param value The default value of this config value.
 */
xmmsc_result_t *
xmmsc_configval_register (xmmsc_connection_t *c, const char *key,
                          const char *value)
{
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_REGVALUE);
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_string (msg, value);

	return xmmsc_send_msg (c, msg);
}

/**
 * Sets a configvalue in the server.
 *
 * @param c The connection structure.
 * @param key The key of the configval to set a value for.
 * @param val The new value of the configval.
 */
xmmsc_result_t *
xmmsc_configval_set (xmmsc_connection_t *c, const char *key, const char *val)
{
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_SETVALUE);
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_string (msg, val);

	return xmmsc_send_msg (c, msg);
}

/**
 * Retrives a list of configvalues in server
 *
 * @param c The connection structure.
 * @param key The key of the configval to retrieve.
 */
xmmsc_result_t *
xmmsc_configval_get (xmmsc_connection_t *c, const char *key)
{
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_GETVALUE);
	xmms_ipc_msg_put_string (msg, key);

	return xmmsc_send_msg (c, msg);
}

/**
 * Lists all configuration values.
 *
 * @param c The connection structure.
 */
xmmsc_result_t *
xmmsc_configval_list (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_LISTVALUES);
}


/**
 * Requests the configval_changed broadcast. This will be called when a configvalue
 * has been updated.
 *
 * @param c The connection structure.
 */
xmmsc_result_t *
xmmsc_broadcast_configval_changed (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED);
}

/** @} */
