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
do_methodcall (xmmsc_connection_t *conn, guint id, const gchar *arg)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;
	
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_MEDIALIB, id);
	xmms_ipc_msg_put_string (msg, arg);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

xmmsc_result_t *
xmmsc_medialib_select (xmmsc_connection_t *conn, const char *query)
{
	return do_methodcall (conn, XMMS_IPC_CMD_SELECT, query);
}

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

xmmsc_result_t *
xmmsc_medialib_add_entry (xmmsc_connection_t *conn, const char *url)
{
	return do_methodcall (conn, XMMS_IPC_CMD_ADD, url);
}

xmmsc_result_t *
xmmsc_medialib_playlist_save_current (xmmsc_connection_t *conn,
                                      const char *name)
{
	return do_methodcall (conn, XMMS_IPC_CMD_PLAYLIST_SAVE_CURRENT, name);
}

xmmsc_result_t *
xmmsc_medialib_playlist_load (xmmsc_connection_t *conn,
                                      const char *name)
{
	return do_methodcall (conn, XMMS_IPC_CMD_PLAYLIST_LOAD, name);
}

xmmsc_result_t *
xmmsc_medialib_path_import (xmmsc_connection_t *conn,
			    const char *path)
{
	return do_methodcall (conn, XMMS_IPC_CMD_PATH_IMPORT, path);
}

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

static void
hash_insert (const void *key, const void *value, void *udata)
{
	x_hash_t *hash = udata;
	x_hash_insert (hash, strdup ((char *)key), strdup ((char *)value));
}

x_hash_t *
xmmscs_medialib_get_info (xmmsc_connection_t *c, unsigned int id)
{
	xmmsc_result_t *res;
	x_hash_t *hash, *ret;

	res = xmmsc_medialib_get_info (c, id);
	if (!res)
		return NULL;

	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		return NULL;
	}

	if (!xmmsc_result_get_hashtable (res, &hash)) {
		xmmsc_result_unref (res);
		return NULL;
	}

	ret = x_hash_new_full (x_str_hash, x_str_equal, g_free, g_free);

	x_hash_foreach (hash, hash_insert, ret);

	xmmsc_result_unref (res);
	return ret;
}

xmmsc_result_t *
xmmsc_broadcast_medialib_entry_changed (xmmsc_connection_t *c)
{
	return xmmsc_send_broadcast_msg (c, XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
}

/**
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


