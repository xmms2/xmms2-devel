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




#ifndef __XMMSCLIENT_INT_H__
#define __XMMSCLIENT_INT_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#ifdef XMMS_OS_DARWIN
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "xmms/ipc_msg.h"

#include "internal/xhash-int.h"
#include "internal/xlist-int.h"
#include "internal/client_ipc.h"


/**
 * @typedef xmmsc_connection_t
 *
 * Holds all data about the current connection to
 * the XMMS server.
 */

struct xmmsc_connection_St {
	int ref;
	
	xmmsc_ipc_t *ipc;

	x_hash_t *callbacks;
	x_hash_t *replies;
	char *error;
	int timeout;
	void *data;

	char *clientname;
};

void xmmsc_ref (xmmsc_connection_t *c);

xmmsc_result_t *xmmsc_send_msg_no_arg (xmmsc_connection_t *c, int object, int cmd);
xmmsc_result_t *xmmsc_send_msg (xmmsc_connection_t *c, xmms_ipc_msg_t *msg);
xmmsc_result_t *xmmsc_send_msg_flush (xmmsc_connection_t *c, xmms_ipc_msg_t *msg);
x_hash_t * xmmsc_deserialize_hashtable (xmms_ipc_msg_t *msg);
x_hash_t * xmmsc_deserialize_mediainfo (xmms_ipc_msg_t *msg);
xmmsc_result_t * xmmsc_send_broadcast_msg (xmmsc_connection_t *c, uint32_t signalid);
xmmsc_result_t * xmmsc_send_signal_msg (xmmsc_connection_t *c, uint32_t signalid);

#endif

