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
 * @defgroup PlaybackControl PlaybackControl
 * @ingroup XMMSClient
 * @brief This controls the playback.
 *
 * @{
 */

/**
 * Stop decoding of current song. This will start decoding of the song
 * set with xmmsc_playlist_set_next, or the current song again if no
 * xmmsc_playlist_set_next was executed.
 */
xmmsc_result_t *
xmmsc_playback_tickle (xmmsc_connection_t *c)
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

	return res;
}

/**
 * Requests the playback status broadcast. This will be called when
 * events like play, stop and pause is triggered.
 */
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

/**
 * Request the current id signal. This will be called then the 
 * current playing id is changed. New song for example.
 */
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

/**
 * Request the playback_playtime signal. Will update the
 * time we have played the current entry.
 */
xmmsc_result_t *
xmmsc_signal_playback_playtime (xmmsc_connection_t *c)
{
	return xmmsc_send_signal_msg (c, XMMS_IPC_SIGNAL_OUTPUT_PLAYTIME);
}

/**
 * Make server emit the current playtime.
 */
xmmsc_result_t *
xmmsc_playback_playtime (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_OUTPUT, XMMS_IPC_CMD_CPLAYTIME);
}

/** @} */

