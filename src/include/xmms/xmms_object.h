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




#ifndef __XMMS_OBJECT_H__
#define __XMMS_OBJECT_H__

#include <glib.h>
#include "xmms/xmms_error.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_coll.h"

#define XMMS_OBJECT_MID 0x00455574

struct xmms_object_St;
typedef struct xmms_object_St xmms_object_t;
typedef struct xmms_object_cmd_desc_St xmms_object_cmd_desc_t;

typedef void (*xmms_object_destroy_func_t) (xmms_object_t *object);

/** @addtogroup Object
  * @{
  */
struct xmms_object_St {
	guint32 id;
	GMutex *mutex;

	GList *signals[XMMS_IPC_SIGNAL_END];
	xmms_object_cmd_desc_t *cmds[XMMS_IPC_CMD_END];

	gint ref;
	xmms_object_destroy_func_t destroy_func;
};

/** @} */

typedef void (*xmms_object_handler_t) (xmms_object_t *object, gconstpointer data, gpointer userdata);

typedef struct {
	union {
		gint32 int32;
		guint32 uint32;
		gchar *string;
		GTree *dict;
		GHashTable *hash;
		GList *list;
		xmmsc_coll_t *coll;
		GString *bin;
	} value;
	xmms_object_cmd_arg_type_t type;
	gint refcount;
} xmms_object_cmd_value_t;

xmms_object_cmd_value_t *xmms_object_cmd_value_str_new (const gchar *string);
xmms_object_cmd_value_t *xmms_object_cmd_value_bin_new (GString *bin);
xmms_object_cmd_value_t *xmms_object_cmd_value_uint_new (guint32 uint);
xmms_object_cmd_value_t *xmms_object_cmd_value_int_new (gint32 i);
xmms_object_cmd_value_t *xmms_object_cmd_value_dict_new (GTree *dict);
xmms_object_cmd_value_t *xmms_object_cmd_value_hash_table_new (GHashTable *hash);
xmms_object_cmd_value_t *xmms_object_cmd_value_list_new (GList *list);
xmms_object_cmd_value_t *xmms_object_cmd_value_propdict_new (GList *list);
xmms_object_cmd_value_t *xmms_object_cmd_value_coll_new (xmmsc_coll_t *coll);
xmms_object_cmd_value_t *xmms_object_cmd_value_none_new (void);
xmms_object_cmd_value_t *xmms_object_cmd_value_ref (xmms_object_cmd_value_t *val);
void xmms_object_cmd_value_unref (xmms_object_cmd_value_t *val);

#define XMMS_OBJECT_CMD_MAX_ARGS 6
typedef struct {
	xmms_object_cmd_value_t values[XMMS_OBJECT_CMD_MAX_ARGS];
	xmms_object_cmd_value_t *retval;
	xmms_error_t error;
} xmms_object_cmd_arg_t;

typedef void (*xmms_object_cmd_func_t) (xmms_object_t *object, xmms_object_cmd_arg_t *arg);

struct xmms_object_cmd_desc_St {
	xmms_object_cmd_func_t func;
	xmms_object_cmd_arg_type_t retval;
	xmms_object_cmd_arg_type_t args[XMMS_OBJECT_CMD_MAX_ARGS];
/*	xmms_object_cmd_arg_type_t arg2;
	xmms_object_cmd_arg_type_t arg3;
	xmms_object_cmd_arg_type_t arg4;*/
};

#define XMMS_OBJECT(p) ((xmms_object_t *)p)
#define XMMS_IS_OBJECT(p) (XMMS_OBJECT (p)->id == XMMS_OBJECT_MID)

void xmms_object_cleanup (xmms_object_t *object);

void xmms_object_parent_set (xmms_object_t *object, xmms_object_t *parent);

void xmms_object_connect (xmms_object_t *object, guint32 signalid,
			  xmms_object_handler_t handler, gpointer userdata);

void xmms_object_disconnect (xmms_object_t *object, guint32 signalid,
			     xmms_object_handler_t handler, gpointer userdata);

void xmms_object_emit (xmms_object_t *object, guint32 signalid,
		       gconstpointer data);

void xmms_object_emit_f (xmms_object_t *object, guint32 signalid,
			 xmms_object_cmd_arg_type_t type, ...);

void xmms_object_cmd_arg_init (xmms_object_cmd_arg_t *arg);

void xmms_object_cmd_add (xmms_object_t *object, guint cmdid, xmms_object_cmd_desc_t *desc);

void xmms_object_cmd_call (xmms_object_t *object, guint cmdid, xmms_object_cmd_arg_t *arg);

/* Some minor macro-magic. XMMS_CMD_DEFINE and XMMS_CMD_FUNC
 * are the only ones to be used directly */

#define __XMMS_CMD_DO_ARG_NONE(a)
#define __XMMS_CMD_DO_ARG_STRING(a) ,arg->values[a].value.string
#define __XMMS_CMD_DO_ARG_UINT32(a) ,arg->values[a].value.uint32
#define __XMMS_CMD_DO_ARG_INT32(a) ,arg->values[a].value.int32
#define __XMMS_CMD_DO_ARG_STRINGLIST(a) ,arg->values[a].value.list
#define __XMMS_CMD_DO_ARG_COLL(a) ,arg->values[a].value.coll
#define __XMMS_CMD_DO_ARG_BIN(a) ,arg->values[a].value.bin
#define __XMMS_CMD_DO_RETVAL_NONE() arg->retval = xmms_object_cmd_value_none_new();
#define __XMMS_CMD_DO_RETVAL_DICT() arg->retval = xmms_object_cmd_value_dict_new
#define __XMMS_CMD_DO_RETVAL_UINT32() arg->retval = xmms_object_cmd_value_uint_new
#define __XMMS_CMD_DO_RETVAL_INT32() arg->retval = xmms_object_cmd_value_int_new
#define __XMMS_CMD_DO_RETVAL_LIST() arg->retval = xmms_object_cmd_value_list_new
#define __XMMS_CMD_DO_RETVAL_PROPDICT() arg->retval = xmms_object_cmd_value_propdict_new
#define __XMMS_CMD_DO_RETVAL_STRING() arg->retval = xmms_object_cmd_value_str_new
#define __XMMS_CMD_DO_RETVAL_COLL() arg->retval = xmms_object_cmd_value_coll_new
#define __XMMS_CMD_DO_RETVAL_BIN() arg->retval = xmms_object_cmd_value_bin_new

#define XMMS_CMD_DEFINE6(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, argtype3, argtype4, argtype5, argtype6) static void \
__int_xmms_cmd_##cmdid (xmms_object_t *object, xmms_object_cmd_arg_t *arg) \
{ \
g_return_if_fail (XMMS_IS_OBJECT (object)); \
g_return_if_fail (arg->values[0].type == XMMS_OBJECT_CMD_ARG_##argtype1); \
g_return_if_fail (arg->values[1].type == XMMS_OBJECT_CMD_ARG_##argtype2); \
g_return_if_fail (arg->values[2].type == XMMS_OBJECT_CMD_ARG_##argtype3); \
g_return_if_fail (arg->values[3].type == XMMS_OBJECT_CMD_ARG_##argtype4); \
g_return_if_fail (arg->values[4].type == XMMS_OBJECT_CMD_ARG_##argtype5); \
g_return_if_fail (arg->values[5].type == XMMS_OBJECT_CMD_ARG_##argtype6); \
__XMMS_CMD_DO_RETVAL_##_rettype() (realfunc ((argtype0)object __XMMS_CMD_DO_ARG_##argtype1(0) __XMMS_CMD_DO_ARG_##argtype2(1) __XMMS_CMD_DO_ARG_##argtype3(2) __XMMS_CMD_DO_ARG_##argtype4(3) __XMMS_CMD_DO_ARG_##argtype5(4) __XMMS_CMD_DO_ARG_##argtype6(5), &arg->error)); \
} \
xmms_object_cmd_desc_t __int_xmms_cmd_desc_##cmdid = { __int_xmms_cmd_##cmdid, XMMS_OBJECT_CMD_ARG_##_rettype, {XMMS_OBJECT_CMD_ARG_##argtype1, XMMS_OBJECT_CMD_ARG_##argtype2, XMMS_OBJECT_CMD_ARG_##argtype3, XMMS_OBJECT_CMD_ARG_##argtype4, XMMS_OBJECT_CMD_ARG_##argtype5, XMMS_OBJECT_CMD_ARG_##argtype6} }

#define XMMS_CMD_DEFINE(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2) XMMS_CMD_DEFINE6(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, NONE, NONE, NONE, NONE)
#define XMMS_CMD_DEFINE3(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, argtype3) XMMS_CMD_DEFINE6(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, argtype3, NONE, NONE, NONE)
#define XMMS_CMD_DEFINE4(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, argtype3, argtype4) XMMS_CMD_DEFINE6(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, argtype3, argtype4, NONE, NONE)
#define XMMS_CMD_DEFINE5(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, argtype3, argtype4, argtype5) XMMS_CMD_DEFINE6(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, argtype3, argtype4, argtype5, NONE)

#define XMMS_CMD_FUNC(cmdid) &__int_xmms_cmd_desc_##cmdid


void __int_xmms_object_unref (xmms_object_t *object);
xmms_object_t *__int_xmms_object_new (gint size, xmms_object_destroy_func_t destfunc);

#define xmms_object_ref(obj) do { \
	if (obj && XMMS_IS_OBJECT (obj)) { \
		XMMS_OBJECT (obj)->ref++; \
	} \
} while (0)

#define xmms_object_unref(obj) do { \
	if (obj && XMMS_IS_OBJECT (obj)) { \
		__int_xmms_object_unref (XMMS_OBJECT (obj)); \
	} \
} while (0)

#define xmms_object_new(objtype,destroyfunc) (objtype *) __int_xmms_object_new (sizeof (objtype), destroyfunc)

#endif /* __XMMS_OBJECT_H__ */
