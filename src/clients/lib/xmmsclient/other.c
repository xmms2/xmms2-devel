/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_util.h"

/**
 * @defgroup OtherControl OtherControl
 * @ingroup XMMSClient
 * @brief This controls various other functions of the XMMS server.
 *
 * @{
 */

/**
 * Registers a config property in the server.
 * @param key should be <clientname>.myval like cli.path or something like that.
 * @param val The default value of this config property.
 */
xmmsc_result_t *
xmmsc_configval_register (xmmsc_connection_t *c, const char *key,
                          const char *val)
{
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_CMD_REGVALUE);
	xmms_ipc_msg_put_string (msg, key);
	xmms_ipc_msg_put_string (msg, val);

	return xmmsc_send_msg (c, msg);
}

/**
 * Sets a configvalue in the server.
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
 * Request the visualisation data signal. This will be called with vis data
 */
xmmsc_result_t *
xmmsc_signal_visualisation_data (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_signal_msg (c, XMMS_IPC_SIGNAL_VISUALISATION_DATA);
}

/**
 * Request status for the mediainfo reader. It can be idle or working
 */
xmmsc_result_t *
xmmsc_broadcast_mediainfo_reader_status (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS);
}

/**
 * Request number of unindexed entries in medialib.
 */
xmmsc_result_t *
xmmsc_signal_mediainfo_reader_unindexed (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_signal_msg (c, XMMS_IPC_SIGNAL_MEDIAINFO_READER_UNINDEXED);
}

/**
 * Browse available media in a path.
 *
 * Retrieves a list of paths available (directly) under the specified
 * path.
 *
 */
xmmsc_result_t *
xmmsc_xform_media_browse (xmmsc_connection_t *c, const char *url)
{
	char *enc_url;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	enc_url = _xmmsc_medialib_encode_url (url, 0, NULL);
	if (!enc_url)
		return NULL;

	res = xmmsc_xform_media_browse_encoded (c, enc_url);

	free (enc_url);

	return res;

}

/**
 * Browse available media in a (already encoded) path.
 *
 * Retrieves a list of paths available (directly) under the specified
 * path.
 *
 */
xmmsc_result_t *
xmmsc_xform_media_browse_encoded (xmmsc_connection_t *c, const char *url)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!url, "with a NULL url", NULL);

	if (!_xmmsc_medialib_verify_url (url))
		x_api_error ("with a non encoded url", NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_XFORM, XMMS_IPC_CMD_BROWSE);
	xmms_ipc_msg_put_string (msg, url);
	res = xmmsc_send_msg (c, msg);

	return res;

}

/**
 * Get the absolute path to the user config dir.
 * @sa xmms_userconfdir_get()
 *
 * @param buf A char buffer
 * @param len The length of buf (PATH_MAX is a good choice)
 * @return A pointer to buf, or NULL if an error occurred.
 */
const char *
xmmsc_userconfdir_get (char *buf, int len)
{
	return xmms_userconfdir_get(buf, len);
}

/** @} */


