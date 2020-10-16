/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient.h>
#include <xmmsclientpriv/xmmsclient_ipc.h>
#include <xmmsc/xmmsc_idnumbers.h>

/**
 * @defgroup StatsFunctions StatsFunctions
 * @ingroup XMMSClient
 * @brief This module contains functions to access information and statistics of the XMMS server.
 *
 * @{
 */

/**
 * Get a list of loaded plugins from the server
 */
xmmsc_result_t *
xmmsc_main_list_plugins (xmmsc_connection_t *c, xmms_plugin_type_t type)
{
	x_check_conn (c, NULL);

	return xmmsc_send_cmd (c, XMMS_IPC_OBJECT_MAIN, XMMS_IPC_COMMAND_MAIN_LIST_PLUGINS,
	                       XMMSV_LIST_ENTRY_INT (type), XMMSV_LIST_END);
}

/**
 * Get a list of statistics from the server
 */
xmmsc_result_t *
xmmsc_main_stats (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_MAIN, XMMS_IPC_COMMAND_MAIN_STATS);
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

/** @} */
