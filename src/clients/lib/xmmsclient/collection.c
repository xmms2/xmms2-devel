/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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
#include "xmmsc/xmmsc_idnumbers.h"


static void xmmsc_coll_msg_put_string_list (xmms_ipc_msg_t *msg, const char* strings[]);


/**
 * @defgroup Collection Collection
 * @ingroup XMMSClient
 * @brief This performs everything related to collections.
 *
 * @{
 */

/* [xmmsc_coll_t] */
xmmsc_result_t*
xmmsc_coll_get (xmmsc_connection_t *conn, char *collname,
                xmmsc_coll_namespace_t ns)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	/* FIXME: command */
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_COLLECTION, XMMS_IPC_CMD_COLLECTION_GET);
	xmms_ipc_msg_put_string (msg, collname);
	xmms_ipc_msg_put_string (msg, ns);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/* [list<string>] */
xmmsc_result_t*
xmmsc_coll_list (xmmsc_connection_t *conn, xmmsc_coll_namespace_t ns)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	/* FIXME: command */
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_COLLECTION, XMMS_IPC_CMD_COLLECTION_LIST);
	xmms_ipc_msg_put_string (msg, ns);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/* [void] */
xmmsc_result_t*
xmmsc_coll_save (xmmsc_connection_t *conn, xmmsc_coll_t *coll,
                 char* name, xmmsc_coll_namespace_t ns)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	/* FIXME: command */
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_COLLECTION, XMMS_IPC_CMD_COLLECTION_SAVE);
	xmms_ipc_msg_put_string (msg, name);
	xmms_ipc_msg_put_string (msg, ns);
	xmms_ipc_msg_put_collection (msg, coll);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/* [void] */
xmmsc_result_t*
xmmsc_coll_remove (xmmsc_connection_t *conn,
                   char* name, xmmsc_coll_namespace_t ns)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	/* FIXME: command */
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_COLLECTION, XMMS_IPC_CMD_COLLECTION_REMOVE);
	xmms_ipc_msg_put_string (msg, name);
	xmms_ipc_msg_put_string (msg, ns);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

 
/* Search (in collections) */
/* [list<xmmsc_coll_t>] */
xmmsc_result_t*
xmmsc_coll_find (xmmsc_connection_t *conn, unsigned int mediaid, xmmsc_coll_namespace_t ns)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	/* FIXME: command */
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_COLLECTION, XMMS_IPC_CMD_COLLECTION_FIND);
	xmms_ipc_msg_put_uint32 (msg, mediaid);
	xmms_ipc_msg_put_string (msg, ns);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

 
/* Query */
/* [list<uint>] */
xmmsc_result_t*
xmmsc_coll_query_ids (xmmsc_connection_t *conn, xmmsc_coll_t *coll, 
                      const char* order[], unsigned int limit_start,
                      unsigned int limit_len)
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	/* FIXME: command */
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_COLLECTION, XMMS_IPC_CMD_QUERY_IDS);
	xmms_ipc_msg_put_collection (msg, coll);
	xmms_ipc_msg_put_uint32 (msg, limit_start);
	xmms_ipc_msg_put_uint32 (msg, limit_len);

	xmmsc_coll_msg_put_string_list (msg, order);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/* [list<Dict>] */
xmmsc_result_t*
xmmsc_coll_query_infos (xmmsc_connection_t *conn, xmmsc_coll_t *coll,
                        const char* order[], unsigned int limit_start,
                        unsigned int limit_len, const char* fetch[],
                        const char* group[])
{
	xmmsc_result_t *res;
	xmms_ipc_msg_t *msg;

	x_check_conn (conn, NULL);

	/* FIXME: command */
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_COLLECTION, XMMS_IPC_CMD_QUERY_INFOS);
	xmms_ipc_msg_put_collection (msg, coll);
	xmms_ipc_msg_put_uint32 (msg, limit_start);
	xmms_ipc_msg_put_uint32 (msg, limit_len);
	xmmsc_coll_msg_put_string_list (msg, order);
	xmmsc_coll_msg_put_string_list (msg, fetch);
	xmmsc_coll_msg_put_string_list (msg, group);

	res = xmmsc_send_msg (conn, msg);

	return res;
}

/** @} */



/** @internal */

/**
 * Given a NULL-terminated array of strings, append the array length
 * and content to the #xmms_ipc_msg_t.
 *
 * @param msg the #xmms_ipc_msg_t to append things to.
 * @param strings the NULL-terminated array of strings.
 */
static void
xmmsc_coll_msg_put_string_list (xmms_ipc_msg_t *msg, const char* strings[])
{
	int n;

	/* FIXME: nicer way? */
	for (n = 0; strings[n] != NULL; n++) { }
	xmms_ipc_msg_put_uint32 (msg, n);

	for (n = 0; strings[n] != NULL; n++) {
		xmms_ipc_msg_put_string (msg, strings[n]);
	}
}
