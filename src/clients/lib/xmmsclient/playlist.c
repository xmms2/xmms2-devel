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
#include "xmmsc/xmmsc_stdbool.h"
#include "xmmsc/xmmsc_stringport.h"

static char *xmmsc_playlist_encode_url (const char *url);

/**
 * @defgroup PlaylistControl PlaylistControl
 * @ingroup XMMSClient
 * @brief This controls the playlist.
 *
 * @{
 */

/**
 * Retrive the current position in the playlist
 */
xmmsc_result_t *
xmmsc_playlist_current_pos (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_CURRENT_POS);
}


/**
 * Shuffles the current playlist.
 */
xmmsc_result_t *
xmmsc_playlist_shuffle (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_SHUFFLE);
}

/**
 * Sorts the playlist according to the property
 */
xmmsc_result_t *
xmmsc_playlist_sort (xmmsc_connection_t *c, char *property)
{
	xmms_ipc_msg_t *msg;
	xmmsc_result_t *res;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_SORT);
	xmms_ipc_msg_put_string (msg, property);

	res = xmmsc_send_msg (c, msg);
	
	return res;
}

/**
 * Clears the current playlist.
 */
xmmsc_result_t *
xmmsc_playlist_clear (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_CLEAR);
}

/**
 * This will make the server list the current playlist.
 * The entries will be feed to the XMMS_SIGNAL_PLAYLIST_LIST
 * callback.
 */
xmmsc_result_t *
xmmsc_playlist_list (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_LIST);
}

/**
 * Insert a medialib id at given position in playlist. 
 *
 * @param c The connection structure.
 * @param pos A position in the playlist
 * @param id A medialib id.
 *
 */
xmmsc_result_t *
xmmsc_playlist_insert_id (xmmsc_connection_t *c, int pos, unsigned int id)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_INSERT_ID);
	xmms_ipc_msg_put_uint32 (msg, pos);
	xmms_ipc_msg_put_uint32 (msg, id);
	res = xmmsc_send_msg (c, msg);

	return res;
}

/**
 * Insert entry at given position in playlist.
 *
 * @param c The connection structure.
 * @param pos A position in the playlist
 * @param url The URL to insert
 *
 */
xmmsc_result_t *
xmmsc_playlist_insert (xmmsc_connection_t *c, int pos, char *url)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_INSERT);
	xmms_ipc_msg_put_uint32 (msg, pos);
	xmms_ipc_msg_put_string (msg, url);
	res = xmmsc_send_msg (c, msg);

	return res;
}



/**
 * Add a medialib id to the playlist. 
 *
 * @param c The connection structure.
 * @param id A medialib id.
 *
 */
xmmsc_result_t *
xmmsc_playlist_add_id (xmmsc_connection_t *c, unsigned int id)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_ADD_ID);
	xmms_ipc_msg_put_uint32 (msg, id);
	res = xmmsc_send_msg (c, msg);

	return res;
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
 */
xmmsc_result_t *
xmmsc_playlist_add (xmmsc_connection_t *c, const char *url)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	char *enc_url;

	enc_url = xmmsc_playlist_encode_url (url);
	if (!enc_url)
		return NULL;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_ADD);
	xmms_ipc_msg_put_string (msg, enc_url);
	res = xmmsc_send_msg (c, msg);

	free (enc_url);

	return res;
}

/**
 * Move a playlist entry relative to it's current postion.
 * eg move (id, -1) will move id one step *up* in the playlist.
 */
xmmsc_result_t *
xmmsc_playlist_move (xmmsc_connection_t *c, unsigned int id, signed int moves)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_MOVE);
	xmms_ipc_msg_put_uint32 (msg, id);
	xmms_ipc_msg_put_int32 (msg, moves);
	res = xmmsc_send_msg (c, msg);

	return res;

}

/**
 * Remove an entry from the playlist.
 *
 * @param pos The position that should be removed from the playlist.
 *
 * @sa xmmsc_playlist_list
 */
xmmsc_result_t *
xmmsc_playlist_remove (xmmsc_connection_t *c, unsigned int pos)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_REMOVE);
	xmms_ipc_msg_put_uint32 (msg, pos);
	res = xmmsc_send_msg (c, msg);

	return res;

}

/**
 * Request the playlist changed broadcast from the server. Everytime someone
 * manipulate the playlist this will be emitted.
 */
xmmsc_result_t *
xmmsc_broadcast_playlist_changed (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
}

/**
 * Request the playlist current pos broadcast. When the position
 * in the playlist is changed this will be called.
 */
xmmsc_result_t *
xmmsc_broadcast_playlist_current_pos (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);
}

/**
 * Set next entry in the playlist. Alter the position pointer.
 */
xmmsc_result_t *
xmmsc_playlist_set_next (xmmsc_connection_t *c, unsigned int pos)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_SET_POS);
	xmms_ipc_msg_put_uint32 (msg, pos);

	res = xmmsc_send_msg (c, msg);

	return res;
}

/**
 * Same as #xmmsc_playlist_set_next but relative to the current postion.
 * -1 will back one and 1 will move to the next.
 */
xmmsc_result_t *
xmmsc_playlist_set_next_rel (xmmsc_connection_t *c, signed int pos)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_SET_POS_REL);
	xmms_ipc_msg_put_uint32 (msg, pos);

	res = xmmsc_send_msg (c, msg);

	return res;
}

static int
free_str (void * key, void * value, void * udata)
{
	char *k = (char *)key;

	if (strcasecmp (k, "id") == 0) {
		free (key);
		return true;
	}

	if (key)
		free (key);
	if (value)
		free (value);

	return true;
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

/** @} */

#define GOODCHAR(a) ((((a) >= 'a') && ((a) <= 'z')) || \
                     (((a) >= 'A') && ((a) <= 'Z')) || \
                     (((a) >= '0') && ((a) <= '9')) || \
                     ((a) == ':') || \
                     ((a) == '/') || \
                     ((a) == '-') || \
                     ((a) == '.') || \
                     ((a) == '_'))

static char *
xmmsc_playlist_encode_url (const char *url)
{
	static char hex[16] = "0123456789abcdef";
	int i = 0, j = 0;
	char *res;

	res = malloc (strlen(url) * 3 + 1);
	if (!res)
		return NULL;

	while (url[i]) {
		unsigned char chr = url[i++];
		if (GOODCHAR (chr)) {
			res[j++] = chr;
		} else if (chr == ' ') {
			res[j++] = '+';
		} else {
			res[j++] = '%';
			res[j++] = hex[((chr & 0xf0) >> 4)];
			res[j++] = hex[(chr & 0x0f)];
		}
	}

	res[j] = '\0';

	return res;
}
