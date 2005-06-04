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
 * @defgroup MedialibControl MedialibControl
 * @ingroup XMMSClient
 * @brief This controls the medialib.
 *
 * @{
 */

static xmmsc_result_t *
do_methodcall (xmmsc_connection_t *conn, unsigned int id, const char *arg)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, id);
	xmms_ipc_msg_put_string (msg, arg);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Make a SQL query to the server medialib. The result will contain
 * a #x_list_t with #x_hash_t's.
 * @param conn The #xmmsc_connection_t
 * @param query The SQL query.
 */
xmmsc_result_t *
xmmsc_medialib_select (xmmsc_connection_t *conn, const char *query)
{
	return do_methodcall (conn, XMMS_IPC_CMD_SELECT, query);
}

/**
 * Search for a entry (URL) in the medialib db and return its ID number
 * @param conn The #xmmsc_connection_t
 * @param url The URL to search for
 */
xmmsc_result_t *
xmmsc_medialib_get_id (xmmsc_connection_t *conn, const char *url)
{
	return do_methodcall (conn, XMMS_IPC_CMD_GET_ID, url);
}

/**
 * Export a serverside playlist to a format that could be read
 * from another mediaplayer.
 * @param conn The #xmmsc_connection_t
 * @param playlist Name of a serverside playlist
 * @param mime Mimetype of the export format.
 */
xmmsc_result_t *
xmmsc_medialib_playlist_export (xmmsc_connection_t *conn, const char *playlist, const char *mime)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_PLAYLIST_EXPORT);
	xmms_ipc_msg_put_string (msg, playlist);
	xmms_ipc_msg_put_string (msg, mime);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Import a playlist from a playlist file.
 * @param conn The #xmmsc_connection_t
 * @param playlist The name of the new playlist.
 * @param url URL to the playlist file.
 */
xmmsc_result_t *
xmmsc_medialib_playlist_import (xmmsc_connection_t *conn, const char *playlist, const char *url)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_PLAYLIST_IMPORT);
	xmms_ipc_msg_put_string (msg, playlist);
	xmms_ipc_msg_put_string (msg, url);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Remove a entry from the medialib
 * @param conn The #xmmsc_connection_t
 * @param entry The entry id you want to remove
 */
xmmsc_result_t *
xmmsc_medialib_remove_entry (xmmsc_connection_t *conn, int32_t entry)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_REMOVE);
	xmms_ipc_msg_put_uint32 (msg, entry);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/**
 * Add a URL to the medialib. If you want to add mutiple files
 * you should call #xmmsc_medialib_path_import
 * @param conn The #xmmsc_connection_t
 * @param url URL to add to the medialib.
 */
xmmsc_result_t *
xmmsc_medialib_add_entry (xmmsc_connection_t *conn, const char *url)
{
	return do_methodcall (conn, XMMS_IPC_CMD_ADD, url);
}

/**
 * Save the current playlist to a serverside playlist
 */
xmmsc_result_t *
xmmsc_medialib_playlist_save_current (xmmsc_connection_t *conn,
                                      const char *name)
{
	return do_methodcall (conn, XMMS_IPC_CMD_PLAYLIST_SAVE_CURRENT, name);
}

/**
 * Load a playlist from the medialib to the current active playlist
 */
xmmsc_result_t *
xmmsc_medialib_playlist_load (xmmsc_connection_t *conn,
                                      const char *name)
{
	return do_methodcall (conn, XMMS_IPC_CMD_PLAYLIST_LOAD, name);
}

/**
 * Import a all files recursivly from the directory passed
 * as argument.
 * @param conn #xmmsc_connection_t
 * @param path A directory to recursive search for mediafiles
 */
xmmsc_result_t *
xmmsc_medialib_path_import (xmmsc_connection_t *conn,
			    const char *path)
{
	return do_methodcall (conn, XMMS_IPC_CMD_PATH_IMPORT, path);
}

/**
 * Rehash the medialib, this will check data in the medialib
 * still is the same as the data in files.
 * @param id The id to rehash. Set it to 0 if you want to rehash
 * the whole medialib.
 */
xmmsc_result_t *
xmmsc_medialib_rehash (xmmsc_connection_t *conn,
		       unsigned int id)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_REHASH);
	xmms_ipc_msg_put_uint32 (msg, id);

	res = xmmsc_send_msg (conn, msg);

	return res;

}

/**
 * Retrieve information about a entry from the medialib.
 */
xmmsc_result_t *
xmmsc_medialib_get_info (xmmsc_connection_t *c, unsigned int id)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_INFO);
	xmms_ipc_msg_put_uint32 (msg, id);

	res = xmmsc_send_msg (c, msg);

	return res;
}

/**
 * Request the medialib_entry_changed broadcast. This will be called
 * if a entry changes on the serverside. The argument will be an medialib
 * id.
 */
xmmsc_result_t *
xmmsc_broadcast_medialib_entry_changed (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
}

/**
 * Queries the medialib for files and adds the matching ones to
 * the current playlist. Remember to include a field called id
 * in the query.
 *
 * @param c The connection structure.
 * @param query sql-query to medialib.
 *
 */
xmmsc_result_t *
xmmsc_medialib_add_to_playlist (xmmsc_connection_t *c, char *query)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, XMMS_IPC_CMD_ADD_TO_PLAYLIST);
	xmms_ipc_msg_put_string (msg, query);
	res = xmmsc_send_msg (c, msg);

	return res;

}

/** @} */
