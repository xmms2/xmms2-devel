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

#include "xmms/xmmsclient.h"
#include "xmms/signal_xmms.h"

#include "internal/xmmsclient_int.h"


/**
 * @defgroup PlaybackControl PlaybackControl
 * @ingroup XMMSClient
 * @brief This controls the playback.
 *
 * @{
 */

/**
 * Plays the next track in playlist.
 */

xmmsc_result_t *
xmmsc_playback_next (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_DECODER_KILL);
}

/**
 * Stops the current playback. This will make the server
 * idle.
 */

xmmsc_result_t *
xmmsc_playback_stop (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_STOP);
}

/**
 * Pause the current playback, will tell the output to not read
 * nor write.
 */

xmmsc_result_t *
xmmsc_playback_pause (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_PAUSE);
}

/**
 * Starts playback if server is idle.
 */

xmmsc_result_t *
xmmsc_playback_start (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_START);
}



/**
 * Seek to a absolute time in the current playback.
 *
 * @param c The connection structure.
 * @param milliseconds The total number of ms where
 * playback should continue.
 */

xmmsc_result_t *
xmmsc_playback_seek_ms (xmmsc_connection_t *c, unsigned int milliseconds)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_SEEKMS);
	xmms_ipc_msg_put_uint32 (msg, milliseconds);

	res = xmmsc_send_msg (c, msg);
	xmms_ipc_msg_destroy (msg);

	return res;
}

/**
 * Seek to a absoulte number of samples in the current playback.
 *
 * @param c The connection structure.
 * @param samples the total number of samples where playback
 * should continue.
 */

xmmsc_result_t *
xmmsc_playback_seek_samples (xmmsc_connection_t *c, unsigned int samples)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_SEEKSAMPLES);
	xmms_ipc_msg_put_uint32 (msg, samples);

	res = xmmsc_send_msg (c, msg);
	xmms_ipc_msg_destroy (msg);

	return res;
}


xmmsc_result_t *
xmmsc_broadcast_playback_status (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_OUTPUT_STATUS);
}

/**
 * Make server emit the playback status.
 */

xmmsc_result_t *
xmmsc_playback_status (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_STATUS);
}

xmmsc_result_t *
xmmsc_broadcast_playback_current_id (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_OUTPUT_CURRENTID);
}

/**
 * Make server emit the current id.
 */

xmmsc_result_t *
xmmsc_playback_current_id (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_CURRENTID);
}

unsigned int
xmmscs_playback_current_id (xmmsc_connection_t *c)
{
	int ret = 0;
	xmmsc_result_t *res;

	res = xmmsc_playback_current_id (c);
	if (!res)
		return 0;

	xmmsc_result_wait (res);

	if (!xmmsc_result_get_uint (res, &ret)) {
		xmmsc_result_unref (res);
		return 0;
	}

	xmmsc_result_unref (res);

	return ret;

}

xmmsc_result_t *
xmmsc_signal_playback_playtime (xmmsc_connection_t *c)
{
	return xmmsc_send_signal_msg (c, XMMS_IPC_SIGNAL_OUTPUT_PLAYTIME);
}

xmmsc_result_t *
xmmsc_playback_playtime (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_CPLAYTIME);
}

int
xmmscs_playback_playtime (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;
	int ret;

	res = xmmsc_playback_playtime (c);
	if (!res)
		return 0;

	xmmsc_result_wait (res);
	if (!xmmsc_result_get_uint (res, &ret)) {
		xmmsc_result_unref (res);
		return 0;
	}

	xmmsc_result_unref (res);

	return ret;
}

/** @} */

