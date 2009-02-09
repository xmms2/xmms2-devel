/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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
#include "xmmsc/xmmsv.h"
#include "xmmsc/xmmsv_coll.h"

#define XMMS_OBJECT_MID 0x00455574

G_BEGIN_DECLS

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

	GTree *signals;
	GTree *cmds;

	gint ref;
	xmms_object_destroy_func_t destroy_func;
};


/* Convenience wrapper to create #xmmsv_t from GLib types. */
xmmsv_t *xmms_convert_and_kill_list (GList *list);
xmmsv_t *xmms_convert_and_kill_dict (GTree *dict);
xmmsv_t *xmms_convert_and_kill_string (gchar *str);
xmmsv_t *xmms_convert_and_kill_bin (GString *gs);

int xmms_bin_to_gstring (xmmsv_t *value, GString **gs);
int dummy_identity (xmmsv_t *value, xmmsv_t **arg);
gboolean check_string_list (xmmsv_t *list);


/** @} */

typedef void (*xmms_object_handler_t) (xmms_object_t *object, xmmsv_t *data, gpointer userdata);

#define XMMS_OBJECT_CMD_MAX_ARGS 6
typedef struct {
	xmmsv_t *values[XMMS_OBJECT_CMD_MAX_ARGS];
	xmmsv_t *retval;
	xmms_error_t error;
} xmms_object_cmd_arg_t;

typedef void (*xmms_object_cmd_func_t) (xmms_object_t *object, xmms_object_cmd_arg_t *arg);

struct xmms_object_cmd_desc_St {
	xmms_object_cmd_func_t func;
	xmmsv_type_t retval;
	xmmsv_type_t args[XMMS_OBJECT_CMD_MAX_ARGS];
};

#define XMMS_OBJECT(p) ((xmms_object_t *)p)
#define XMMS_IS_OBJECT(p) (XMMS_OBJECT (p)->id == XMMS_OBJECT_MID)

void xmms_object_cleanup (xmms_object_t *object);

void xmms_object_parent_set (xmms_object_t *object, xmms_object_t *parent);

void xmms_object_connect (xmms_object_t *object, guint32 signalid,
			  xmms_object_handler_t handler, gpointer userdata);

void xmms_object_disconnect (xmms_object_t *object, guint32 signalid,
			     xmms_object_handler_t handler, gpointer userdata);

void xmms_object_emit (xmms_object_t *object, guint32 signalid, xmmsv_t *data);

void xmms_object_emit_f (xmms_object_t *object, guint32 signalid,
			 xmmsv_type_t type, ...);

void xmms_object_cmd_arg_init (xmms_object_cmd_arg_t *arg);

void xmms_object_cmd_add (xmms_object_t *object, guint cmdid, xmms_object_cmd_desc_t *desc);

void xmms_object_cmd_call (xmms_object_t *object, guint cmdid, xmms_object_cmd_arg_t *arg);

/* Some minor macro-magic. XMMS_CMD_DEFINE and XMMS_CMD_FUNC
 * are the only ones to be used directly */

#define __XMMS_CMD_INIT_ARG_FULL(argn, argtypecode, extract_func) \
	argtypecode argval##argn; \
	g_return_if_fail (extract_func (arg->values[argn], &argval##argn));

#define __XMMS_CMD_INIT_ARG(argn, argtype, argtypecode) \
	__XMMS_CMD_INIT_ARG_FULL(argn, argtypecode, xmmsv_get_##argtype)

#define __XMMS_CMD_INIT_ARG_NONE(a)
#define __XMMS_CMD_INIT_ARG_STRING(a) __XMMS_CMD_INIT_ARG(a, string, const gchar *)
#define __XMMS_CMD_INIT_ARG_INT32(a)  __XMMS_CMD_INIT_ARG(a, int,  gint)
#define __XMMS_CMD_INIT_ARG_COLL(a)   __XMMS_CMD_INIT_ARG(a, coll, xmmsv_coll_t *)
#define __XMMS_CMD_INIT_ARG_BIN(a)    __XMMS_CMD_INIT_ARG_FULL(a, GString *, xmms_bin_to_gstring)
#define __XMMS_CMD_INIT_ARG_LIST(a)   __XMMS_CMD_INIT_ARG_FULL(a, xmmsv_t *, dummy_identity)
#define __XMMS_CMD_INIT_ARG_DICT(a)   __XMMS_CMD_INIT_ARG_FULL(a, xmmsv_t *, dummy_identity)

#define __XMMS_CMD_PRINT_ARG_NONE(a)
#define __XMMS_CMD_PRINT_ARG_STRING(a) , argval##a
#define __XMMS_CMD_PRINT_ARG_INT32(a)  , argval##a
#define __XMMS_CMD_PRINT_ARG_COLL(a)   , argval##a
#define __XMMS_CMD_PRINT_ARG_BIN(a)    , argval##a
#define __XMMS_CMD_PRINT_ARG_LIST(a)   , argval##a
#define __XMMS_CMD_PRINT_ARG_DICT(a)   , argval##a

#define __XMMS_CMD_DO_RETVAL_NONE() arg->retval = xmmsv_new_none();
#define __XMMS_CMD_DO_RETVAL_DICT() arg->retval = xmms_convert_and_kill_dict
#define __XMMS_CMD_DO_RETVAL_INT32() arg->retval = xmmsv_new_int
#define __XMMS_CMD_DO_RETVAL_LIST() arg->retval = xmms_convert_and_kill_list
#define __XMMS_CMD_DO_RETVAL_STRING() arg->retval = xmms_convert_and_kill_string
#define __XMMS_CMD_DO_RETVAL_COLL() arg->retval = xmmsv_new_coll
#define __XMMS_CMD_DO_RETVAL_BIN() arg->retval =

#define XMMS_CMD_DEFINE6(cmdid, realfunc, argtype0, _rettype, argtype1, argtype2, argtype3, argtype4, argtype5, argtype6) static void \
__int_xmms_cmd_##cmdid (xmms_object_t *object, xmms_object_cmd_arg_t *arg) \
{ \
g_return_if_fail (XMMS_IS_OBJECT (object)); \
__XMMS_CMD_INIT_ARG_##argtype1 (0) \
__XMMS_CMD_INIT_ARG_##argtype2 (1) \
__XMMS_CMD_INIT_ARG_##argtype3 (2) \
__XMMS_CMD_INIT_ARG_##argtype4 (3) \
__XMMS_CMD_INIT_ARG_##argtype5 (4) \
__XMMS_CMD_INIT_ARG_##argtype6 (5) \
__XMMS_CMD_DO_RETVAL_##_rettype() (realfunc ((argtype0)object __XMMS_CMD_PRINT_ARG_##argtype1(0) __XMMS_CMD_PRINT_ARG_##argtype2(1) __XMMS_CMD_PRINT_ARG_##argtype3(2) __XMMS_CMD_PRINT_ARG_##argtype4(3) __XMMS_CMD_PRINT_ARG_##argtype5(4) __XMMS_CMD_PRINT_ARG_##argtype6(5), &arg->error)); \
} \
xmms_object_cmd_desc_t __int_xmms_cmd_desc_##cmdid = { __int_xmms_cmd_##cmdid, XMMSV_TYPE_##_rettype, {XMMSV_TYPE_##argtype1, XMMSV_TYPE_##argtype2, XMMSV_TYPE_##argtype3, XMMSV_TYPE_##argtype4, XMMSV_TYPE_##argtype5, XMMSV_TYPE_##argtype6} }


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

G_END_DECLS

#endif /* __XMMS_OBJECT_H__ */
