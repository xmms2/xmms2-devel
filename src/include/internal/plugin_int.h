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




#ifndef __XMMS_PLUGIN_INT_H__
#define __XMMS_PLUGIN_INT_H__

#include "xmms/config.h"

/*
 * Private functions
 */

gboolean xmms_plugin_init (gchar *path);
void xmms_plugin_shutdown ();
void xmms_plugin_scan_directory (const gchar *dir);

GList *xmms_plugin_list_get (xmms_plugin_type_t type);
void xmms_plugin_list_destroy (GList *list);

xmms_plugin_t *xmms_plugin_find (xmms_plugin_type_t type, const gchar *name);

xmms_plugin_method_t xmms_plugin_method_get (xmms_plugin_t *plugin,
											 const gchar *member);


#endif
