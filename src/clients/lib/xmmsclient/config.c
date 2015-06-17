/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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


#include <xmmsclient/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient_ipc.h>
#include <xmmsc/xmmsc_idnumbers.h>

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
xmmsc_config_register_value (xmmsc_connection_t *c, const char *key,
                             const char *value)
{
	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_CONFIG,
	                       XMMS_IPC_COMMAND_CONFIG_REGISTER_VALUE,
	                       XMMSV_LIST_ENTRY_STR (key),
	                       XMMSV_LIST_ENTRY_STR (value),
	                       XMMSV_LIST_END);
}

/**
 * Sets a configvalue in the server.
 *
 * @param c The connection structure.
 * @param key The key of the configval to set a value for.
 * @param val The new value of the configval.
 */
xmmsc_result_t *
xmmsc_config_set_value (xmmsc_connection_t *c,
                        const char *key, const char *val)
{
	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_CONFIG,
	                       XMMS_IPC_COMMAND_CONFIG_SET_VALUE,
	                       XMMSV_LIST_ENTRY_STR (key),
	                       XMMSV_LIST_ENTRY_STR (val),
	                       XMMSV_LIST_END);
}

/**
 * Retrieves a configvalue from the server
 *
 * @param c The connection structure.
 * @param key The key of the configval to retrieve.
 */
xmmsc_result_t *
xmmsc_config_get_value (xmmsc_connection_t *c, const char *key)
{
	x_check_conn (c, NULL);
	x_api_error_if (!key, "with a NULL key", NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_CONFIG,
	                       XMMS_IPC_COMMAND_CONFIG_GET_VALUE,
	                       XMMSV_LIST_ENTRY_STR (key), XMMSV_LIST_END);
}

/**
 * Lists all configuration values.
 *
 * @param c The connection structure.
 */
xmmsc_result_t *
xmmsc_config_list_values (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_CONFIG, XMMS_IPC_COMMAND_CONFIG_LIST_VALUES);
}

/**
 * Requests the config_value_changed broadcast. This will be called when a configvalue
 * has been updated.
 *
 * @param c The connection structure.
 */
xmmsc_result_t *
xmmsc_broadcast_config_value_changed (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_CONFIG_VALUE_CHANGED);
}

/** @} */
