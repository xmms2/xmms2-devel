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
#include "xmms/xmmsclient-result.h"
#include "xmms/signal_xmms.h"

#include "internal/xmmsclient_int.h"
#include "internal/xhash-int.h"
#include "internal/xlist-int.h"


/**
 * @defgroup PlaylistControl PlaylistControl
 * @ingroup XMMSClient
 * @brief This controls the playlist.
 *
 * @{
 */

/**
 * Shuffles the current playlist.
 */

xmmsc_result_t *
xmmsc_playlist_shuffle (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_PLAYLIST, XMMS_METHOD_SHUFFLE);
}

/**
 * Sorts the playlist according to the property
 */

xmmsc_result_t *
xmmsc_playlist_sort (xmmsc_connection_t *c, char *property)
{
	DBusMessageIter itr;
	DBusMessage *msg;
	xmmsc_result_t *res;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_SORT);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, property);

	res = xmmsc_send_msg (c, msg);

	dbus_message_unref (msg);

	return res;
}

/**
 * Clears the current playlist.
 */

xmmsc_result_t *
xmmsc_playlist_clear (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_PLAYLIST, XMMS_METHOD_CLEAR);
}

/**
 * Saves the playlist to the specified filename. The
 * format of the playlist is detemined by checking the
 * extension of the filename.
 *
 * @param c The connection structure.
 * @param filename file on server-side to save the playlist
 * in.
 */

xmmsc_result_t *
xmmsc_playlist_save (xmmsc_connection_t *c, char *filename)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	xmmsc_result_t *res;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_SAVE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, filename);
	res = xmmsc_send_msg (c, msg);
	dbus_message_unref (msg);

	return res;
}

/**
 * This will make the server list the current playlist.
 * The entries will be feed to the XMMS_SIGNAL_PLAYLIST_LIST
 * callback.
 */

xmmsc_result_t *
xmmsc_playlist_list (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_OBJECT_PLAYLIST, XMMS_METHOD_LIST);
}

x_list_t *
xmmscs_playlist_list (xmmsc_connection_t *c)
{
	int i;
	xmmsc_result_t *res;
	x_list_t *list;

	res = xmmsc_playlist_list (c);
	if (!res)
		return NULL;

	xmmsc_result_wait (res);

	i = xmmsc_result_get_uintlist (res, &list);

	xmmsc_result_unref (res);

	if (i) return list;
	return NULL;
}

/**
 * Add the url to the playlist. The url should be encoded with
 * xmmsc_encode_path and be absolute to the server-side. Note that
 * you will have to include the protocol for the url to. ie:
 * file://mp3/my_mp3s/first.mp3.
 *
 * @param c The connection structure.
 * @param url an encoded path.
 *
 * @sa xmmsc_encode_path
 */

xmmsc_result_t *
xmmsc_playlist_add (xmmsc_connection_t *c, char *url)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	xmmsc_result_t *res;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_ADD);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, url);
	res = xmmsc_send_msg (c, msg);
	dbus_message_unref (msg);

	return res;

}

/**
 * Remove an entry from the playlist.
 *
 * param id the id to remove from the playlist.
 *
 * @sa xmmsc_playlist_list
 */

xmmsc_result_t *
xmmsc_playlist_remove (xmmsc_connection_t *c, unsigned int id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	xmmsc_result_t *res;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_REMOVE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	res = xmmsc_send_msg (c, msg);
	dbus_message_unref (msg);

	return res;

}

xmmsc_result_t *
xmmsc_playlist_changed (xmmsc_connection_t *c)
{
	DBusMessage *msg;
	DBusMessageIter itr;
	xmmsc_result_t *ret;

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_CLIENT, XMMS_DBUS_INTERFACE, XMMS_METHOD_ONCHANGE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, XMMS_SIGNAL_PLAYLIST_CHANGED);
	ret = xmmsc_send_on_change (c, msg);
	dbus_message_unref (msg);

	xmmsc_result_restartable (ret, c, XMMS_SIGNAL_PLAYLIST_CHANGED);

	return ret;
}

xmmsc_result_t *
xmmsc_playlist_entry_changed (xmmsc_connection_t *c)
{
	DBusMessage *msg;
	DBusMessageIter itr;
	xmmsc_result_t *ret;

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_CLIENT, XMMS_DBUS_INTERFACE, XMMS_METHOD_ONCHANGE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID);
	ret = xmmsc_send_on_change (c, msg);
	dbus_message_unref (msg);

	xmmsc_result_restartable (ret, c, XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID);

	return ret;
}


/**
 * Retrives information about a certain entry.
 */
xmmsc_result_t *
xmmsc_playlist_get_mediainfo (xmmsc_connection_t *c, unsigned int id)
{
	DBusMessage *msg;
	DBusMessageIter itr;
	xmmsc_result_t *res;

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_GETMEDIAINFO);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);

	res = xmmsc_send_msg (c, msg);

	dbus_message_unref (msg);

	return res;
}

x_hash_t *
xmmscs_playlist_get_mediainfo (xmmsc_connection_t *c, unsigned int id)
{
	xmmsc_result_t *res;
	x_hash_t *ret;

	res = xmmsc_playlist_get_mediainfo (c, id);
	if (!res)
		return NULL;

	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		return NULL;
	}

	if (!xmmsc_result_get_mediainfo (res, &ret)) {
		xmmsc_result_unref (res);
		return NULL;
	}

	xmmsc_result_unref (res);
	return ret;
}

static int
free_str (void * key, void * value, void * udata)
{
	char *k = (char *)key;

	if (strcasecmp (k, "id") == 0) {
		free (key);
		return TRUE;
	}

	if (key)
		free (key);
	if (value)
		free (value);

	return TRUE;
}

/**
 * Free all strings in a x_hash_t.
 */
void
xmmsc_playlist_entry_free (x_hash_t *entry)
{
	if (entry) {
		x_hash_foreach_remove (entry, free_str, NULL);
		x_hash_destroy (entry);
	}
}

/**
 * @todo fix the integers here, this will b0rk b0rk if we
 * change the enum in core.
 */

/** @} */


