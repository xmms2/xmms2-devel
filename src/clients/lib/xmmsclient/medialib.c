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

#include "xmms/xmmsclient.h"
#include "xmms/xmmsclient-result.h"
#include "xmms/signal_xmms.h"

#include "internal/xmmsclient_int.h"
#include "internal/xhash-int.h"
#include "internal/xlist-int.h"


/**
 * @defgroup MedialibControl MedialibControl
 * @ingroup XMMSClient
 * @brief This controls the medialib.
 *
 * @{
 */

xmmsc_result_t *
xmmsc_medialib_select (xmmsc_connection_t *conn, const char *query)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_SELECT);
	xmms_ipc_msg_put_string (msg, query);

	res = xmmsc_send_msg (conn, msg);
	xmms_ipc_msg_destroy (msg);

	return res;
}

/** @} */


