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

void
xmmsc_playlist_shuffle (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_OBJECT_PLAYLIST, XMMS_METHOD_SHUFFLE);
}

/**
 * Sorts the playlist according to the property
 */

void
xmmsc_playlist_sort (xmmsc_connection_t *c, char *property)
{
	DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_SORT);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, property);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * Clears the current playlist.
 */

void
xmmsc_playlist_clear (xmmsc_connection_t *c)
{
	xmmsc_send_void(c, XMMS_OBJECT_PLAYLIST, XMMS_METHOD_CLEAR);
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

void
xmmsc_playlist_save (xmmsc_connection_t *c, char *filename)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_SAVE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, filename);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);
}

/**
 * This will make the server list the current playlist.
 * The entries will be feed to the XMMS_SIGNAL_PLAYLIST_LIST
 * callback.
 */

void
xmmsc_playlist_list (xmmsc_connection_t *c)
{
	int cserial;

	cserial = xmmsc_send_void(c, XMMS_OBJECT_PLAYLIST, XMMS_METHOD_LIST);
	xmmsc_connection_add_reply (c, cserial, XMMS_SIGNAL_PLAYLIST_LIST);
}

unsigned int *
xmmscs_playlist_list (xmmsc_connection_t *c)
{
	DBusMessage *msg,*rmsg;
        DBusMessageIter itr;
	DBusError err;
	unsigned int *arr;

	dbus_error_init (&err);

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_LIST);

	rmsg = dbus_connection_send_with_reply_and_block (c->conn, msg, c->timeout, &err);

	if (rmsg) {
		dbus_message_iter_init (rmsg, &itr);

		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
			unsigned int len = dbus_message_iter_get_uint32 (&itr);
			if (len > 0) {
				int len;
				unsigned int *tmp;

				dbus_message_iter_next (&itr);
				dbus_message_iter_get_uint32_array (&itr, &tmp, &len);

				arr = malloc (sizeof (unsigned int) * len+1);
				memcpy (arr, tmp, len * sizeof(unsigned int));
				arr[len] = '\0';
			} else {
				arr = malloc (sizeof (unsigned int));
				*arr = -1;
			}
		}

		dbus_message_unref (rmsg);
	} else {
		printf ("Error: %s\n", err.message);
	}

	dbus_message_unref (msg);

	return arr;
}

/**
 * Set the current song in the playlist.
 *
 * @param c The connection structure.
 * @param id The id that should be the current song
 * in the playlist.
 *
 * @sa xmmsc_playlist_list
 */

void
xmmsc_playlist_jump (xmmsc_connection_t *c, unsigned int id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_JUMP);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

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

void
xmmsc_playlist_add (xmmsc_connection_t *c, char *url)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_ADD);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_string (&itr, url);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

}

/**
 * Remove an entry from the playlist.
 *
 * param id the id to remove from the playlist.
 *
 * @sa xmmsc_playlist_list
 */

void
xmmsc_playlist_remove (xmmsc_connection_t *c, unsigned int id)
{
        DBusMessageIter itr;
	DBusMessage *msg;
	int cserial;
	
	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_REMOVE);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);
	dbus_message_unref (msg);

}




/**
 * Retrives information about a certain entry.
 */
void
xmmsc_playlist_get_mediainfo (xmmsc_connection_t *c, unsigned int id)
{
	DBusMessage *msg;
	DBusMessageIter itr;
	DBusError err;
	unsigned int cserial;

	dbus_error_init (&err);

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_GETMEDIAINFO);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	dbus_connection_send (c->conn, msg, &cserial);

	xmmsc_connection_add_reply (c, cserial, XMMS_SIGNAL_PLAYLIST_MEDIAINFO);

	dbus_message_unref (msg);
	dbus_connection_flush (c->conn);
}

x_hash_t *
xmmscs_playlist_get_mediainfo (xmmsc_connection_t *c, unsigned int id)
{
	DBusMessage *msg, *rmsg;
	DBusMessageIter itr;
	DBusError err;
	x_hash_t *res = NULL;

	dbus_error_init (&err);

	msg = dbus_message_new_method_call (NULL, XMMS_OBJECT_PLAYLIST, XMMS_DBUS_INTERFACE, XMMS_METHOD_GETMEDIAINFO);
	dbus_message_append_iter_init (msg, &itr);
	dbus_message_iter_append_uint32 (&itr, id);
	rmsg = dbus_connection_send_with_reply_and_block (c->conn, msg, c->timeout, &err);

	if (rmsg) {
		dbus_message_iter_init (rmsg, &itr);
		res = xmmsc_deserialize_mediainfo (&itr);
		dbus_message_unref (rmsg);
	} else {
		printf ("error: %s", err.message);
	}

	dbus_message_unref (msg);
	dbus_connection_flush (c->conn);

	return res;

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
	x_hash_foreach_remove (entry, free_str, NULL);

	x_hash_destroy (entry);
}

/**
 * @todo fix the integers here, this will b0rk b0rk if we
 * change the enum in core.
 */

/** @} */


