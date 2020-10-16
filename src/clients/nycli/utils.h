/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>

gint xmmsv_strcmp (xmmsv_t **a, xmmsv_t **b);
gboolean xmmsv_propdict_lengths (xmmsv_t *properties, gint *proplen, gint *srclen);
xmmsv_t *xmmsv_coll_intersect_with_playlist (xmmsv_t *coll, const gchar *playlist);
xmmsv_t *xmmsv_coll_apply_default_order (xmmsv_t *query);
xmmsv_t *xmmsv_coll_from_stdin (void);
void xmmsv_print_value (const gchar *source, const gchar *key, xmmsv_t *val);

void print_padding (gint length, const gchar padchar);

gchar *format_time (guint64 duration, gboolean use_hours);
void enrich_mediainfo (xmmsv_t *val);

gchar *decode_url (const gchar *string);
gchar *encode_url (gchar *url);
gchar *format_url (const gchar *path, GFileTest test);
void breakdown_timespan (int64_t span, gint *days, gint *hours, gint *minutes, gint *seconds);

#endif /* __UTILS_H__ */
