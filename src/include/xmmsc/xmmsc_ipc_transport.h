/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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


#ifndef XMMS_IPC_TRANSPORT_H
#define XMMS_IPC_TRANSPORT_H

#include <xmmsc/xmmsc_stdint.h>
#include <xmmsc/xmmsc_sockets.h>

typedef struct xmms_ipc_transport_St xmms_ipc_transport_t;

typedef int (*xmms_ipc_read_func) (xmms_ipc_transport_t *, char *, int);
typedef int (*xmms_ipc_write_func) (xmms_ipc_transport_t *, char *, int);
typedef xmms_ipc_transport_t *(*xmms_ipc_accept_func) (xmms_ipc_transport_t *);
typedef void (*xmms_ipc_destroy_func) (xmms_ipc_transport_t *);

void xmms_ipc_transport_destroy (xmms_ipc_transport_t *ipct);
int xmms_ipc_transport_read (xmms_ipc_transport_t *ipct, char *buffer, int len);
int xmms_ipc_transport_write (xmms_ipc_transport_t *ipct, char *buffer, int len);
xmms_socket_t xmms_ipc_transport_fd_get (xmms_ipc_transport_t *ipct);
xmms_ipc_transport_t * xmms_ipc_server_accept (xmms_ipc_transport_t *ipct);
xmms_ipc_transport_t * xmms_ipc_client_init (const char *path);
xmms_ipc_transport_t * xmms_ipc_server_init (const char *path);
char * xmms_ipc_hostname (const char *path);

struct xmms_ipc_transport_St {
	char *path;
	void *data;
	xmms_socket_t fd;
	int32_t peer;
	int16_t peer_port;

	xmms_ipc_accept_func accept_func;
	xmms_ipc_write_func write_func;
	xmms_ipc_read_func read_func;
	xmms_ipc_destroy_func destroy_func;
};

#endif

