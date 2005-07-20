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




#ifndef __XMMS_CONFIG_H__
#define __XMMS_CONFIG_H__

typedef struct xmms_config_St xmms_config_t;
typedef struct xmms_config_value_St xmms_config_value_t;

#include "xmms/xmms_object.h"

xmms_config_value_t *xmms_config_lookup (const gchar *path);

const gchar *xmms_config_value_string_get (const xmms_config_value_t *val);
gint xmms_config_value_int_get (const xmms_config_value_t *val);
gfloat xmms_config_value_float_get (const xmms_config_value_t *val);
const gchar *xmms_config_value_name_get (const xmms_config_value_t *value);

xmms_config_value_t *xmms_config_value_register (const gchar *path, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);

void xmms_config_value_data_set (xmms_config_value_t *val, const gchar *data);

void xmms_config_value_callback_set (xmms_config_value_t *val, xmms_object_handler_t cb, gpointer userdata);
void xmms_config_value_callback_remove (xmms_config_value_t *val, xmms_object_handler_t cb);

#endif
