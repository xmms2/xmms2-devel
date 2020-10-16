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

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"

/**
 * Send a client-to-client message.
 * @param c The connection to the server.
 * @param reply_policy Whether to expect no reply, a single reply or multiple
 * replies for this message.
 * @param payload The contents of the message.
 */
xmmsc_result_t *
xmmsc_c2c_send (xmmsc_connection_t *c, int dest,
                xmms_c2c_reply_policy_t reply_policy, xmmsv_t *payload)
{
	uint32_t cookie;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!dest, "with 0 as dest.", NULL);
	x_api_error_if (!payload, "with NULL payload.", NULL);

	cookie = xmmsc_send_cmd_cookie (c, XMMS_IPC_OBJECT_COURIER,
	                                XMMS_IPC_COMMAND_COURIER_SEND_MESSAGE,
	                                XMMSV_LIST_ENTRY_INT (dest),
	                                XMMSV_LIST_ENTRY_INT (reply_policy),
	                                XMMSV_LIST_ENTRY (xmmsv_ref (payload)),
	                                XMMSV_LIST_END);

	/* Multi-replies behave like broadcasts */
	if (reply_policy == XMMS_C2C_REPLY_POLICY_MULTI_REPLY) {
		res = xmmsc_result_new (c, XMMSC_RESULT_CLASS_BROADCAST, cookie);
	} else {
		res = xmmsc_result_new (c, XMMSC_RESULT_CLASS_DEFAULT, cookie);
	}

	/* If this message does not expect a reply, the client
	 * will get a reply from the server when the message has been sent,
	 * so the result is not a c2c result.
	 */
	if (res && reply_policy != XMMS_C2C_REPLY_POLICY_NO_REPLY) {
		xmmsc_result_c2c_set (res);
	}

	return res;
}

/**
 * Reply to a client-to-client message.
 * @param c The connection to the server.
 * @param msgid The id of the message you are replying to.
 * @param reply_policy Whether to expect no reply, a single reply or multiple
 * replies for this message.
 * @param payload The contents of the reply.
 */
xmmsc_result_t *
xmmsc_c2c_reply (xmmsc_connection_t *c, int msgid,
                 xmms_c2c_reply_policy_t reply_policy, xmmsv_t *payload)
{
	uint32_t cookie;
	xmmsc_result_t *res;

	x_check_conn (c, NULL);
	x_api_error_if (!msgid, "with 0 as msgid.", NULL);
	x_api_error_if (!payload, "with NULL payload.", NULL);

	cookie = xmmsc_send_cmd_cookie (c, XMMS_IPC_OBJECT_COURIER,
	                                XMMS_IPC_COMMAND_COURIER_REPLY,
	                                XMMSV_LIST_ENTRY_INT (msgid),
	                                XMMSV_LIST_ENTRY_INT (reply_policy),
	                                XMMSV_LIST_ENTRY (xmmsv_ref (payload)),
	                                XMMSV_LIST_END);

	if (reply_policy == XMMS_C2C_REPLY_POLICY_MULTI_REPLY) {
		res = xmmsc_result_new (c, XMMSC_RESULT_CLASS_BROADCAST, cookie);
	} else {
		res = xmmsc_result_new (c, XMMSC_RESULT_CLASS_DEFAULT, cookie);
	}

	if (res && reply_policy != XMMS_C2C_REPLY_POLICY_NO_REPLY) {
		xmmsc_result_c2c_set (res);
	}

	return res;
}

/**
 * Request your own client id.
 * @param c The connection to the server.
 */
int32_t
xmmsc_c2c_get_own_id (xmmsc_connection_t *c)
{
	x_check_conn (c, 0);

	return c->id;
}

/**
 * Request a list of connected client ids.
 * @param c The connection to the server.
 */
xmmsc_result_t *
xmmsc_c2c_get_connected_clients (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_COURIER,
	                              XMMS_IPC_COMMAND_COURIER_GET_CONNECTED_CLIENTS);
}

/**
 * Notify the client's api is ready for query
 */
xmmsc_result_t *
xmmsc_c2c_ready (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_COURIER,
	                              XMMS_IPC_COMMAND_COURIER_READY);
}

/**
 * Request a list of clients ready for c2c communication.
 * @param c The connection to the server.
 */
xmmsc_result_t *
xmmsc_c2c_get_ready_clients (xmmsc_connection_t *c)
{
	x_check_conn (c, NULL);

	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_COURIER,
	                              XMMS_IPC_COMMAND_COURIER_GET_READY_CLIENTS);
}

/**
 * Request the client-to-client message broadcast.
 * This broadcast gets triggered when messages from other clients are received.
 * @param c The connection to the server.
 */
xmmsc_result_t *
xmmsc_broadcast_c2c_message (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;

	x_check_conn (c, NULL);

	res = xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_COURIER_MESSAGE);

	if (res) {
		xmmsc_result_c2c_set (res);
	}

	return res;
}

/**
 * Request the client service ready broadcast.
 * This broadcast gets triggered when a client notify the server its api is
 * ready.
 * @param c The connection to the server.
 */
xmmsc_result_t *
xmmsc_broadcast_c2c_ready (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;

	x_check_conn (c, NULL);

	res = xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_COURIER_READY);

	return res;
}

/**
 * Request the client connected broadcast.
 * This broadcast gets triggered when a new client connects, and contains
 * the new client's id.
 * @param c The connection to the server.
 */
xmmsc_result_t *
xmmsc_broadcast_c2c_client_connected (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;

	x_check_conn (c, NULL);

	res = xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_IPC_MANAGER_CLIENT_CONNECTED);

	return res;
}

/**
 * Request the client disconnected broadcast.
 * This broadcast gets triggered when a client disconnects, and contains
 * the disconnected client's id.
 * @param c The connection to the server.
 */
xmmsc_result_t *
xmmsc_broadcast_c2c_client_disconnected (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;

	x_check_conn (c, NULL);

	res = xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_IPC_MANAGER_CLIENT_DISCONNECTED);

	return res;
}
