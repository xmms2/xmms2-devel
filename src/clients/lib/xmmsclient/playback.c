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

#include <dbus/dbus.h>

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
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_OUTPUT, XMMS_METHOD_DECODER_KILL);
}

/**
 * Stops the current playback. This will make the server
 * idle.
 */

xmmsc_result_t *
xmmsc_playback_stop (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_OUTPUT, XMMS_METHOD_STOP);
}

/**
 * Pause the current playback, will tell the output to not read
 * nor write.
 */

xmmsc_result_t *
xmmsc_playback_pause (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_OUTPUT, XMMS_METHOD_PAUSE);
}

/**
 * Starts playback if server is idle.
 */

xmmsc_result_t *
xmmsc_playback_start (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_OUTPUT, XMMS_METHOD_START);
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
        DBusMessageIter itr;
	DBusMessage *msg;
	xmmsc_result_t *res;

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_OUTPUT, XMMS_DBUS_INTERFACE, XMMS_METHOD_SEEKMS);

	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, milliseconds);
	res = xmmsc_send_msg (c, msg);
	dbus_message_unref (msg);
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
        DBusMessageIter itr;
	DBusMessage *msg;
	xmmsc_result_t *res;

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_OUTPUT, XMMS_DBUS_INTERFACE, XMMS_METHOD_SEEKSAMPLES);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, samples);
	res = xmmsc_send_msg (c, msg);
	dbus_message_unref (msg);
	return res;
}

/**
 * Make server emit the current id.
 */

xmmsc_result_t *
xmmsc_playback_status (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;
	res = xmmsc_send_msg_no_arg (c, XMMS_OBJECT_OUTPUT, XMMS_METHOD_STATUS);
	xmmsc_result_restartable (res, c, XMMS_SIGNAL_OUTPUT_STATUS);
	return res;
}

xmmsc_result_t *
xmmsc_playback_current_id (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;
	res = xmmsc_send_msg_no_arg (c, XMMS_OBJECT_OUTPUT, XMMS_METHOD_CURRENTID);
	xmmsc_result_restartable (res, c, XMMS_SIGNAL_OUTPUT_CURRENTID);
	return res;
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
xmmsc_playback_playtime (xmmsc_connection_t *c)
{
	xmmsc_result_t *res;
	res = xmmsc_send_msg_no_arg (c, XMMS_OBJECT_OUTPUT, XMMS_METHOD_CPLAYTIME);
	xmmsc_result_restartable (res, c, XMMS_SIGNAL_OUTPUT_PLAYTIME);
	return res;
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

