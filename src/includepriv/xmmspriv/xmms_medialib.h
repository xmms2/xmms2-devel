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




#ifndef __XMMS_PRIV_MEDIALIB_H__
#define __XMMS_PRIV_MEDIALIB_H__

#include "xmms/xmms_medialib.h"
#include "xmmspriv/xmms_playlist.h"
#include "xmmspriv/xmms_sqlite.h"

typedef struct xmms_medialib_St xmms_medialib_t;

gboolean xmms_medialib_init (xmms_playlist_t *playlist);

xmms_medialib_entry_t xmms_medialib_entry_not_resolved_get (xmms_medialib_session_t *session);
guint xmms_medialib_num_not_resolved (xmms_medialib_session_t *session);
void xmms_medialib_entry_remove (xmms_medialib_session_t *session, xmms_medialib_entry_t entry);

GHashTable *xmms_medialib_entry_to_hashtable (xmms_medialib_session_t *session, xmms_medialib_entry_t entry);
guint xmms_medialib_entry_id_get (xmms_medialib_session_t *session, xmms_medialib_entry_t entry);
gboolean xmms_medialib_entry_is_resolved (xmms_medialib_session_t *session, xmms_medialib_entry_t entry);
void xmms_medialib_playlist_save_autosaved ();
void xmms_medialib_playlist_load_autosaved ();

void xmms_medialib_logging_start (xmms_medialib_session_t *session, xmms_medialib_entry_t entry);
void xmms_medialib_logging_stop (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, guint playtime);
void xmms_medialib_entry_cleanup (xmms_medialib_session_t *session, xmms_medialib_entry_t entry);
xmms_medialib_entry_t xmms_medialib_entry_new_encoded (xmms_medialib_session_t *session, const char *url, xmms_error_t *error);
gboolean xmms_medialib_decode_url (char *url);
gboolean xmms_medialib_check_id (xmms_medialib_entry_t entry);

gboolean xmms_medialib_entry_property_set_str_source (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *property, const gchar *value, guint32 source);
gboolean xmms_medialib_entry_property_set_int_source (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *property, gint value, guint32 source);

#endif
