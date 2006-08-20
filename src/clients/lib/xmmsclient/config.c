/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 Peter Alm, Tobias Rundström, Anders Gustafsson
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
 * @defgroup ConfigControl ConfigControl
 * @ingroup XMMSClient
 * @brief This controls configuration values on the XMMS server.
 *
 * @{
 */

/**
 * Registers a configvalue in the server.
 * @param key should be <clientname>.myval like cli.path or something like that.
 * @param value The default value of this config value.
 */
xmmsc_result_t *
xmmsc_configval_register (xmmsc_connection_t *c, const char *key,
                          const char *value)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_REGVALUE);
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_string (msg, value);
	res = xmmsc_send_msg (c, msg);

	return res;
}

/**
 * Sets a configvalue in the server.
 */
xmmsc_result_t *
xmmsc_configval_set (xmmsc_connection_t *c, const char *key, const char *val)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

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
xmmsc_configval_get (xmmsc_connection_t *c, const char *key)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_GETVALUE);
	xmms_ipc_msg_put_string (msg, key);
	res = xmmsc_send_msg (c, msg);
	
	return res;
}

/**
 * Lists all configuration values.
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
 */
xmmsc_result_t *
xmmsc_broadcast_configval_changed (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED);
}

/**
 * Browse a server plugin media.
 */
xmmsc_result_t *
xmmsc_xform_media_browse (xmmsc_connection_t *c, const char *url)
{
	char *enc_url;
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = xmmsc_medialib_encode_url (url, 0, NULL);
	if (!enc_url)
		return NULL;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_XFORM, XMMS_IPC_CMD_BROWSE);
	xmms_ipc_msg_put_string (msg, enc_url);
	res = xmmsc_send_msg (c, msg);

	free (enc_url);

	return res;

}

/**
 * Get user config dir.
 */
const char *
xmmsc_userconfdir_get (void)
{
	return USERCONFDIR;
}

/** @} */
