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

#ifndef __XMMS_METADATA_MAPPER_H__
#define __XMMS_METADATA_MAPPER_H__

#include <glib.h>

#include <xmms/xmms_xformplugin.h>

gboolean xmms_metadata_mapper_match (GHashTable *table, xmms_xform_t *xform, const gchar *key, const gchar *value, gsize value_length);
GHashTable *xmms_metadata_mapper_init (const xmms_xform_metadata_basic_mapping_t *basic_mapping, gint basic_count, const xmms_xform_metadata_mapping_t *mapping, gint count);

#endif
