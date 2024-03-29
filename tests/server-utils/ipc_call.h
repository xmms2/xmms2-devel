/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#ifndef __IPC_CALL_H__
#define __IPC_CALL_H__

#include <stdarg.h>
#include <glib.h>

#include <xmms/xmms_object.h>
#include <xmmsc/xmmsc_compiler.h>

xmmsv_t *__xmms_ipc_call (xmms_object_t *object, gint cmd, ...) XMMS_SENTINEL(0);
#define XMMS_IPC_CALL(obj, cmd, ...) __xmms_ipc_call (XMMS_OBJECT (obj), cmd, __VA_ARGS__, NULL);

typedef struct xmms_future_St xmms_future_t;

xmms_future_t *__xmms_ipc_check_signal (xmms_object_t *object, gint message, glong delay, glong timeout);

#define XMMS_FUTURE_USEC_DELAY_DEFAULT 75000L
#define XMMS_FUTURE_USEC_TIMEOUT_DEFAULT 2500000L
#define XMMS_IPC_CHECK_SIGNAL(obj, cmd) __xmms_ipc_check_signal (XMMS_OBJECT (obj), cmd, XMMS_FUTURE_USEC_DELAY_DEFAULT, XMMS_FUTURE_USEC_TIMEOUT_DEFAULT);

void xmms_future_free (xmms_future_t *future);
xmmsv_t *xmms_future_await (xmms_future_t *future, gint count);

#endif
