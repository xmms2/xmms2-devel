/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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


#ifndef __XMMSC_IPC_H__
#define __XMMSC_IPC_H__

#include "xmmsc/xmmsc_stdbool.h"
//#include <sys/time.h> Should this be in or out?
#include "xmmsc/xmmsc_ipc_msg.h"
#include "xmmsc/xmmsc_stdint.h"
#include "xmmsc/xmmsc_sockets.h"
#include "xmmsclient/xmmsclient.h"

#ifdef __cplusplus
extern "C" {
#endif

struct xmmsc_ipc_St;
typedef struct xmmsc_ipc_St xmmsc_ipc_t;

typedef void (*xmmsc_ipc_wakeup_t) (xmmsc_ipc_t *);

xmmsc_ipc_t *xmmsc_ipc_init (void);
void xmmsc_ipc_lock_set (xmmsc_ipc_t *ipc, void *lock, void (*lockfunc)(void *), void (*unlockfunc)(void *));
void xmmsc_ipc_disconnect_set (xmmsc_ipc_t *ipc, void (*disconnect_callback) (void *), void *, xmmsc_user_data_free_func_t);
void xmmsc_ipc_need_out_callback_set (xmmsc_ipc_t *ipc, void (*callback) (int, void *), void *userdata, xmmsc_user_data_free_func_t);
bool xmmsc_ipc_msg_write (xmmsc_ipc_t *ipc, xmms_ipc_msg_t *msg, uint32_t cookie);
void xmmsc_ipc_disconnect (xmmsc_ipc_t *ipc);
bool xmmsc_ipc_disconnected (xmmsc_ipc_t *ipc);
void xmmsc_ipc_destroy (xmmsc_ipc_t *ipc);
bool xmmsc_ipc_connect (xmmsc_ipc_t *ipc, char *path);
void xmmsc_ipc_error_set (xmmsc_ipc_t *ipc, char *error);
const char *xmmsc_ipc_error_get (xmmsc_ipc_t *ipc);
xmms_socket_t xmmsc_ipc_fd_get (xmmsc_ipc_t *ipc);

void xmmsc_ipc_result_register (xmmsc_ipc_t *ipc, xmmsc_result_t *res);
xmmsc_result_t *xmmsc_ipc_result_lookup (xmmsc_ipc_t *ipc, uint32_t cookie);
void xmmsc_ipc_result_unregister (xmmsc_ipc_t *ipc, xmmsc_result_t *res);
void xmmsc_ipc_wait_for_event (xmmsc_ipc_t *ipc, unsigned int timeout);

int xmmsc_ipc_io_out (xmmsc_ipc_t *ipc);
int xmmsc_ipc_io_out_callback (xmmsc_ipc_t *ipc);
int xmmsc_ipc_io_in_callback (xmmsc_ipc_t *ipc);

#ifdef __cplusplus
}
#endif

#endif

