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

#ifndef __XMMS_PRIV_FETCH_INFO_H__
#define __XMMS_PRIV_FETCH_INFO_H__

#include <glib.h>
#include "s4.h"

typedef struct xmms_fetch_info_St xmms_fetch_info_t;

struct xmms_fetch_info_St {
	s4_fetchspec_t *fs;
	GHashTable *ft;
};

xmms_fetch_info_t *xmms_fetch_info_new (s4_sourcepref_t *prefs);
void xmms_fetch_info_free (xmms_fetch_info_t *info);
int xmms_fetch_info_add_song_id (xmms_fetch_info_t *info, void *object);
int xmms_fetch_info_add_key (xmms_fetch_info_t *info, void *object, const gchar *key, s4_sourcepref_t *prefs);
int xmms_fetch_info_get_index (xmms_fetch_info_t *info, void *object, const gchar *key);

#endif
