/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

#ifndef __XMMS_INTERFACE_ENTITY_H__
#define __XMMS_INTERFACE_ENTITY_H__

typedef enum {
	XMMSC_SC_INTERFACE_ENTITY_TYPE_NAMESPACE,
	XMMSC_SC_INTERFACE_ENTITY_TYPE_METHOD,
	XMMSC_SC_INTERFACE_ENTITY_TYPE_BROADCAST
} xmmsc_sc_interface_entity_type_t;


/* constructors/utilities for interface entities */
xmmsc_sc_interface_entity_t *xmmsc_sc_interface_entity_new_method (const char *name, const char *docstring, xmmsc_sc_method_t method, xmmsv_t *positional_args, xmmsv_t *named_args, bool va_positional, bool va_named, void *userdata);
xmmsc_sc_interface_entity_t *xmmsc_sc_interface_entity_new_broadcast (const char *name, const char *docstring);
xmmsc_sc_interface_entity_t *xmmsc_sc_interface_entity_new_namespace (const char *name, const char *docstring);

xmmsc_sc_namespace_t *xmmsc_sc_interface_entity_get_namespace (xmmsc_sc_interface_entity_t *ifent);
xmmsc_sc_interface_entity_type_t xmmsc_sc_interface_entity_get_type (xmmsc_sc_interface_entity_t *ifent);

/* broadcasts */
void xmmsc_sc_interface_entity_broadcast_add_id (xmmsc_sc_interface_entity_t *bcast, int id);
xmmsv_t *xmmsc_sc_interface_entity_broadcast_get_ids (xmmsc_sc_interface_entity_t *bcast);
xmmsv_t *xmmsc_sc_interface_entity_broadcast_introspect (xmmsc_sc_interface_entity_t *bcast);

/* methods */
xmmsv_t *xmmsc_sc_interface_entity_method_call (xmmsc_sc_interface_entity_t *method, xmmsv_t *pargs, xmmsv_t *nargs);
xmmsv_t *xmmsc_sc_interface_entity_method_introspect (xmmsc_sc_interface_entity_t *method);

/* namespaces */
void xmmsc_sc_namespace_destroy (xmmsc_sc_namespace_t *nms);
bool xmmsc_sc_namespace_add_child (xmmsc_sc_namespace_t *nms, xmmsc_sc_interface_entity_t *child);
xmmsc_sc_interface_entity_t *xmmsc_sc_namespace_resolve_path (xmmsc_sc_namespace_t *root, xmmsv_t *path, xmmsc_sc_namespace_t **parent);
xmmsv_t *xmmsc_sc_interface_entity_namespace_introspect (xmmsc_sc_interface_entity_t *nms);

#endif
