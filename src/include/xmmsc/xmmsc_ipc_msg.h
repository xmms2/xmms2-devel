/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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

#include "xmmsc/xmmsc_stdint.h"
#include "xmmsc/xmmsc_stdbool.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmsc/xmmsc_ipc_transport.h"
#include "xmmsc/xmmsc_inline.h"
#include "xmmsc/xmmsc_coll.h"

#define XMMS_IPC_MSG_DEFAULT_SIZE 128 /*32768*/
#define XMMS_IPC_MSG_MAX_SIZE 327680
#define XMMS_IPC_MSG_HEAD_LEN 16 /* all but data */

typedef struct xmms_ipc_msg_St xmms_ipc_msg_t;

uint32_t xmms_ipc_msg_get_length (const xmms_ipc_msg_t *msg);
uint32_t xmms_ipc_msg_get_object (const xmms_ipc_msg_t *msg);
uint32_t xmms_ipc_msg_get_cmd (const xmms_ipc_msg_t *msg);
uint32_t xmms_ipc_msg_get_cookie (const xmms_ipc_msg_t *msg);
void xmms_ipc_msg_set_length (xmms_ipc_msg_t *msg, uint32_t len);
void xmms_ipc_msg_set_cookie (xmms_ipc_msg_t *msg, uint32_t cookie);
void xmms_ipc_msg_set_cmd (xmms_ipc_msg_t *msg, uint32_t cmd);
void xmms_ipc_msg_set_object (xmms_ipc_msg_t *msg, uint32_t object);

xmms_ipc_msg_t *xmms_ipc_msg_new (uint32_t object, uint32_t cmd);
xmms_ipc_msg_t * xmms_ipc_msg_alloc (void);
void xmms_ipc_msg_destroy (xmms_ipc_msg_t *msg);

bool xmms_ipc_msg_write_transport (xmms_ipc_msg_t *msg, xmms_ipc_transport_t *transport, bool *disconnected);
bool xmms_ipc_msg_read_transport (xmms_ipc_msg_t *msg, xmms_ipc_transport_t *transport, bool *disconnected);

uint32_t xmms_ipc_msg_put_uint32 (xmms_ipc_msg_t *msg, uint32_t v);
uint32_t xmms_ipc_msg_put_int32 (xmms_ipc_msg_t *msg, int32_t v);
uint32_t xmms_ipc_msg_put_float (xmms_ipc_msg_t *msg, float v);
uint32_t xmms_ipc_msg_put_string (xmms_ipc_msg_t *msg, const char *str);
uint32_t xmms_ipc_msg_put_string_list (xmms_ipc_msg_t *msg, const char* strings[]);
uint32_t xmms_ipc_msg_put_collection (xmms_ipc_msg_t *msg, xmmsc_coll_t *coll);
uint32_t xmms_ipc_msg_put_bin (xmms_ipc_msg_t *msg, const unsigned char *data, unsigned int len);

void xmms_ipc_msg_store_uint32 (xmms_ipc_msg_t *msg, uint32_t offset, uint32_t v);

typedef enum {
	XMMS_IPC_MSG_ARG_TYPE_END,
	XMMS_IPC_MSG_ARG_TYPE_UINT32,
	XMMS_IPC_MSG_ARG_TYPE_INT32,
	XMMS_IPC_MSG_ARG_TYPE_FLOAT,
	XMMS_IPC_MSG_ARG_TYPE_STRING,
	XMMS_IPC_MSG_ARG_TYPE_DATA
} xmms_ipc_msg_arg_type_t;

#define __XMMS_IPC_MSG_DO_IDENTITY_FUNC(type) static inline type *__xmms_ipc_msg_arg_##type (type *arg) {return arg;}
__XMMS_IPC_MSG_DO_IDENTITY_FUNC(int32_t)
__XMMS_IPC_MSG_DO_IDENTITY_FUNC(uint32_t)
__XMMS_IPC_MSG_DO_IDENTITY_FUNC(float)
__XMMS_IPC_MSG_DO_IDENTITY_FUNC(char)
#undef __XMMS_IPC_MSG_DO_IDENTITY_FUNC

#define XMMS_IPC_MSG_UINT32(a) XMMS_IPC_MSG_ARG_TYPE_UINT32, __xmms_ipc_msg_arg_guint32 (a)
#define XMMS_IPC_MSG_INT32(a) XMMS_IPC_MSG_ARG_TYPE_INT32, __xmms_ipc_msg_arg_gint32 (a)
#define XMMS_IPC_MSG_INT64(a) XMMS_IPC_MSG_ARG_TYPE_FLOAT, __xmms_ipc_msg_arg_gint64 (a)
#define XMMS_IPC_MSG_STRING(a,len) XMMS_IPC_MSG_ARG_TYPE_STRING, ((gint)len), __xmms_ipc_msg_arg_char (a)

#define XMMS_IPC_MSG_END XMMS_IPC_MSG_ARG_TYPE_END

bool xmms_ipc_msg_get_uint32 (xmms_ipc_msg_t *msg, uint32_t *v);
bool xmms_ipc_msg_get_int32 (xmms_ipc_msg_t *msg, int32_t *v);
bool xmms_ipc_msg_get_float (xmms_ipc_msg_t *msg, float *v);
bool xmms_ipc_msg_get_string (xmms_ipc_msg_t *msg, char *str, unsigned int maxlen);
bool xmms_ipc_msg_get_string_alloc (xmms_ipc_msg_t *msg, char **buf, unsigned int *len);
bool xmms_ipc_msg_get_collection_alloc (xmms_ipc_msg_t *msg, xmmsc_coll_t **coll);
bool xmms_ipc_msg_get_bin_alloc (xmms_ipc_msg_t *msg, unsigned char **buf, unsigned int *len);

#endif 
