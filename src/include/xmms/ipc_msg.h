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

#ifndef __XMMS_IPC_MSG_H__
#define __XMMS_IPC_MSG_H__

#include <glib.h>

#include "xmms/ringbuf.h"

#define XMMS_IPC_MSG_DATA_SIZE 32768

typedef struct xmms_ipc_msg_St {
	guint32 object;
	guint32 cmd;
	guint32 cid;
	guint16 get_pos, data_length;
	guint8 data[XMMS_IPC_MSG_DATA_SIZE];
} xmms_ipc_msg_t;

xmms_ipc_msg_t *xmms_ipc_msg_new (guint32 object, guint32 cmd);
void xmms_ipc_msg_destroy (xmms_ipc_msg_t *msg);

gboolean xmms_ipc_msg_can_read (xmms_ringbuf_t *ringbuf);
xmms_ipc_msg_t *xmms_ipc_msg_read (xmms_ringbuf_t *ringbuf);
gboolean xmms_ipc_msg_write (xmms_ringbuf_t *ringbuf, const xmms_ipc_msg_t *msg, guint32 cid);
gboolean xmms_ipc_msg_write_fd (gint fd, const xmms_ipc_msg_t *msg, guint32 cid);

gpointer xmms_ipc_msg_put_data (xmms_ipc_msg_t *msg, gconstpointer data, guint len);
gpointer xmms_ipc_msg_put_uint32 (xmms_ipc_msg_t *msg, guint32 v);
gpointer xmms_ipc_msg_put_int32 (xmms_ipc_msg_t *msg, gint32 v);
gpointer xmms_ipc_msg_put_float (xmms_ipc_msg_t *msg, gfloat v);
gpointer xmms_ipc_msg_put_string (xmms_ipc_msg_t *msg, const char *str);
gpointer xmms_ipc_msg_append (xmms_ipc_msg_t *dmsg, xmms_ipc_msg_t *smsg);

typedef enum {
	XMMS_IPC_MSG_ARG_TYPE_END,
	XMMS_IPC_MSG_ARG_TYPE_UINT32,
	XMMS_IPC_MSG_ARG_TYPE_INT32,
	XMMS_IPC_MSG_ARG_TYPE_FLOAT,
	XMMS_IPC_MSG_ARG_TYPE_STRING,
	XMMS_IPC_MSG_ARG_TYPE_DATA,
} xmms_ipc_msg_arg_type_t;

#define __XMMS_IPC_MSG_DO_IDENTITY_FUNC(type) static inline type *__xmms_ipc_msg_arg_##type (type *arg) {return arg;}
__XMMS_IPC_MSG_DO_IDENTITY_FUNC(gint32)
__XMMS_IPC_MSG_DO_IDENTITY_FUNC(guint32)
__XMMS_IPC_MSG_DO_IDENTITY_FUNC(gfloat)
__XMMS_IPC_MSG_DO_IDENTITY_FUNC(char)
#undef __XMMS_IPC_MSG_DO_IDENTITY_FUNC

#define XMMS_IPC_MSG_UINT32(a) XMMS_IPC_MSG_ARG_TYPE_UINT32, __xmms_ipc_msg_arg_guint32 (a)
#define XMMS_IPC_MSG_INT32(a) XMMS_IPC_MSG_ARG_TYPE_INT32, __xmms_ipc_msg_arg_gint32 (a)
#define XMMS_IPC_MSG_INT64(a) XMMS_IPC_MSG_ARG_TYPE_FLOAT, __xmms_ipc_msg_arg_gint64 (a)
#define XMMS_IPC_MSG_STRING(a,len) XMMS_IPC_MSG_ARG_TYPE_STRING, ((gint)len), __xmms_ipc_msg_arg_char (a)

#define XMMS_IPC_MSG_END XMMS_IPC_MSG_ARG_TYPE_END

void xmms_ipc_msg_get_reset (xmms_ipc_msg_t *msg);
#define xmms_ipc_msg_get_cmd(msg) (msg)->cmd
#define xmms_ipc_msg_get_object(msg) (msg)->object
gboolean xmms_ipc_msg_get_uint32 (xmms_ipc_msg_t *msg, guint32 *v);
gboolean xmms_ipc_msg_get_int32 (xmms_ipc_msg_t *msg, gint32 *v);
gboolean xmms_ipc_msg_get_float (xmms_ipc_msg_t *msg, gfloat *v);
gboolean xmms_ipc_msg_get_string (xmms_ipc_msg_t *msg, char *str, guint maxlen);
gboolean xmms_ipc_msg_get_string_alloc (xmms_ipc_msg_t *msg, char **buf, guint *len);
gboolean xmms_ipc_msg_get (xmms_ipc_msg_t *msg, ...);
gboolean xmms_ipc_msg_get_data (xmms_ipc_msg_t *msg, gpointer buf, guint len);

#endif 
