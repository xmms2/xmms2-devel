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
 * Retrive the current position in the playlist
 */
xmmsc_result_t *
xmmsc_playlist_current_pos (xmmsc_connection_t *c)
{
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_CURRENT_POS);
}

unsigned int
xmmscs_playlist_current_pos (xmmsc_connection_t *c)
{
	unsigned int i = 0;
	xmmsc_result_t *res;

	res = xmmsc_playlist_current_pos (c);
	if (!res)
		return 0;

	xmmsc_result_wait (res);

	xmmsc_result_get_uint (res, &i);

	xmmsc_result_unref (res);

	return i;
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
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_SAVE);
	xmms_ipc_msg_put_string (msg, filename);
	res = xmmsc_send_msg (c, msg);

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
	return xmmsc_send_msg_no_arg (c, XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_LIST);
}

x_list_t *
xmmscs_playlist_list (xmmsc_connection_t *c)
{
	int i;
	xmmsc_result_t *res;
	x_list_t *list, *ret = NULL;

	res = xmmsc_playlist_list (c);
	if (!res)
		return NULL;

	xmmsc_result_wait (res);

	i = xmmsc_result_get_uintlist (res, &list);

	if (i) 
		ret = x_list_copy (list);

	xmmsc_result_unref (res);

	if (ret) return ret;
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
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_ADD);
	xmms_ipc_msg_put_string (msg, url);
	res = xmmsc_send_msg (c, msg);

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
 * param id the id to remove from the playlist.
 *
 * @sa xmmsc_playlist_list
 */

xmmsc_result_t *
xmmsc_playlist_remove (xmmsc_connection_t *c, unsigned int id)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_PLAYLIST, XMMS_IPC_CMD_REMOVE);
	xmms_ipc_msg_put_uint32 (msg, id);
	res = xmmsc_send_msg (c, msg);

	return res;

}

xmmsc_result_t *
xmmsc_broadcast_playlist_changed (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
}

xmmsc_result_t *
xmmsc_broadcast_playlist_entry_changed (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_PLAYLIST_MEDIAINFO_ID);
}

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

/** @} */
