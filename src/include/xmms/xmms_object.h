/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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
#include <xmms/xmms_error.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmmsc/xmmsv.h>
#include <xmmsc/xmmsv_coll.h>

#define XMMS_OBJECT_MID 0x00455574

G_BEGIN_DECLS

struct xmms_object_St;
typedef struct xmms_object_St xmms_object_t;

typedef void (*xmms_object_destroy_func_t) (xmms_object_t *object);

/** @addtogroup Object
  * @{
  */
struct xmms_object_St {
	guint32 id;
	GMutex mutex;

	GTree *signals;
	GTree *cmds;

	gint ref;
	xmms_object_destroy_func_t destroy_func;
};


/* Convenience wrapper to create #xmmsv_t from GLib types. */
xmmsv_t *xmms_convert_and_kill_list (GList *list);
xmmsv_t *xmms_convert_and_kill_string (gchar *str);
xmmsv_t *xmms_convert_and_kill_bin (GString *gs);

int xmms_bin_to_gstring (xmmsv_t *value, GString **gs);
int dummy_identity (xmmsv_t *value, xmmsv_t **arg);


/** @} */

typedef void (*xmms_object_handler_t) (xmms_object_t *object, xmmsv_t *data, gpointer userdata);

#define XMMS_OBJECT_CMD_MAX_ARGS 6
typedef struct {
	xmmsv_t *args; /* list */
	xmmsv_t *retval;
	uint32_t cookie;
	gint32 client;
	xmms_error_t error;
} xmms_object_cmd_arg_t;

typedef void (*xmms_object_cmd_func_t) (xmms_object_t *object, xmms_object_cmd_arg_t *arg);

#define XMMS_OBJECT(p) ((xmms_object_t *)p)
#define XMMS_IS_OBJECT(p) (XMMS_OBJECT (p)->id == XMMS_OBJECT_MID)

void xmms_object_cleanup (xmms_object_t *object);

void xmms_object_connect (xmms_object_t *object, guint32 signalid, xmms_object_handler_t handler, gpointer userdata) XMMS_PUBLIC;

void xmms_object_disconnect (xmms_object_t *object, guint32 signalid, xmms_object_handler_t handler, gpointer userdata) XMMS_PUBLIC;

void xmms_object_emit (xmms_object_t *object, guint32 signalid, xmmsv_t *data);

void xmms_object_cmd_arg_init (xmms_object_cmd_arg_t *arg);

void xmms_object_cmd_add (xmms_object_t *object, guint cmdid, const xmms_object_cmd_func_t desc);

void xmms_object_cmd_call (xmms_object_t *object, guint cmdid, xmms_object_cmd_arg_t *arg);

gpointer xmms_object_ref (gpointer obj) XMMS_PUBLIC;

void xmms_object_unref (gpointer obj) XMMS_PUBLIC;

xmms_object_t *__int_xmms_object_new (gint size, xmms_object_destroy_func_t destfunc) XMMS_PUBLIC;


#define xmms_object_new(objtype,destroyfunc) (objtype *) __int_xmms_object_new (sizeof (objtype), destroyfunc)

G_END_DECLS

#endif /* __XMMS_OBJECT_H__ */
