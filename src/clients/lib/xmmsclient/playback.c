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

void
xmmsc_playback_jump (xmmsc_connection_t *c, unsigned int id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYBACK, XMMS_DBUS_INTERFACE, XMMS_METHOD_JUMP);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Plays the next track in playlist.
 */

void
xmmsc_playback_next (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_OBJECT_PLAYBACK, XMMS_METHOD_NEXT);
}

/**
 * Plays the previos track in the playlist.
 */

void
xmmsc_playback_prev (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_OBJECT_PLAYBACK, XMMS_METHOD_PREV);
}

/**
 * Stops the current playback. This will make the server
 * idle.
 */

void
xmmsc_playback_stop (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_OBJECT_PLAYBACK, XMMS_METHOD_STOP);
}

/**
 * Pause the current playback, will tell the output to not read
 * nor write.
 */

void
xmmsc_playback_pause (xmmsc_connection_t *c)
{
	xmmsc_send_void (c, XMMS_OBJECT_PLAYBACK, XMMS_METHOD_PAUSE);
}

/**
 * Starts playback if server is idle.
 */

void
xmmsc_playback_start (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_OBJECT_PLAYBACK, XMMS_METHOD_PLAY);
}


/**
 * Seek to a absolute time in the current playback.
 *
 * @param c The connection structure.
 * @param milliseconds The total number of ms where
 * playback should continue.
 */

void
xmmsc_playback_seek_ms (xmmsc_connection_t *c, unsigned int milliseconds)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYBACK, XMMS_DBUS_INTERFACE, XMMS_METHOD_SEEKMS);

	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, milliseconds);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Seek to a absoulte number of samples in the current playback.
 *
 * @param c The connection structure.
 * @param samples the total number of samples where playback
 * should continue.
 */

void
xmmsc_playback_seek_samples (xmmsc_connection_t *c, unsigned int samples)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYBACK, XMMS_DBUS_INTERFACE, XMMS_METHOD_SEEKSAMPLES);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, samples);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Make server emit the current id.
 */

void
xmmsc_playback_current_id (xmmsc_connection_t *c)
{
	int cserial;

	cserial = xmmsc_send_void (c, XMMS_OBJECT_PLAYBACK, XMMS_METHOD_CURRENTID);
	xmmsc_connection_add_reply (c, cserial, XMMS_SIGNAL_PLAYBACK_CURRENTID);
}

int
xmmscs_playback_current_id (xmmsc_connection_t *c)
{
	DBusMessage *msg,*rmsg;
        DBusMessageIter itr;
	DBusError err;
	int ret = -1;
	
	dbus_error_init (&err);

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYBACK, XMMS_DBUS_INTERFACE, XMMS_METHOD_CURRENTID);

	rmsg = dbus_connection_send_with_reply_and_block (c->conn, msg, c->timeout, &err);

	if (rmsg) {
		dbus_message_iter_init (rmsg, &itr);

		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32)
			ret = dbus_message_iter_get_uint32 (&itr);

		dbus_message_unref (rmsg);
	} else {
		printf ("Error: %s\n", err.message);
	}
	dbus_message_unref (msg);

	return ret;

}

int
xmmsc_playback_current_playtime (xmmsc_connection_t *c)
{
	DBusMessage *msg,*rmsg;
        DBusMessageIter itr;
	DBusError err;
	int ret = -1;

	dbus_error_init (&err);
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYBACK, XMMS_DBUS_INTERFACE, XMMS_METHOD_CPLAYTIME);
	rmsg = dbus_connection_send_with_reply_and_block (c->conn, msg, c->timeout, &err);

	if (rmsg) {
		dbus_message_iter_init (rmsg, &itr);

		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32)
			ret = dbus_message_iter_get_uint32 (&itr);

		dbus_message_unref (rmsg);
	} else {
		printf ("Error: %s\n", err.message);
	}
	dbus_message_unref (msg);

	return ret;
}

/** @} */

