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

#ifndef __XMMS_CONFIG_H__
#define __XMMS_CONFIG_H__

#include <glib.h>
#include "xmms/xmms_object.h"

G_BEGIN_DECLS

typedef struct xmms_config_St xmms_config_t;
typedef struct xmms_config_property_St xmms_config_property_t;

xmms_config_property_t *xmms_config_lookup (const gchar *path);

const gchar *xmms_config_property_get_string (const xmms_config_property_t *prop);
gint xmms_config_property_get_int (const xmms_config_property_t *prop);
gfloat xmms_config_property_get_float (const xmms_config_property_t *prop);
const gchar *xmms_config_property_get_name (const xmms_config_property_t *prop);

xmms_config_property_t *xmms_config_property_register (const gchar *path, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);

void xmms_config_property_set_data (xmms_config_property_t *prop, const gchar *data);

void xmms_config_property_callback_set (xmms_config_property_t *prop, xmms_object_handler_t cb, gpointer userdata);
void xmms_config_property_callback_remove (xmms_config_property_t *prop, xmms_object_handler_t cb, gpointer userdata);

G_END_DECLS

#endif
