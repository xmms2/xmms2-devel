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




#ifndef __XMMS_PLAYLIST_H__
#define __XMMS_PLAYLIST_H__

#include <glib.h>


/*
 * Public definitions
 */

/*
 * Private defintions
 */

#define XMMS_MAX_URI_LEN 1024
#define XMMS_MEDIA_DATA_LEN 1024
#define XMMS_MAX_INT_ATTRIBUTE_LEN 64
#define XMMS_DEFAULT_PARTYSHUFFLE_UPCOMING 10


struct xmms_playlist_St;
typedef struct xmms_playlist_St xmms_playlist_t;

#include "xmms/xmms_error.h"
#include "xmms/xmms_medialib.h"
#include "xmmspriv/xmms_mediainfo.h"

/*
 * Public functions
 */

xmms_playlist_t * xmms_playlist_init (void);

gboolean xmms_playlist_add_id (xmms_playlist_t *playlist, gchar *plname, xmms_medialib_entry_t file, xmms_error_t *error);
gboolean xmms_playlist_advance (xmms_playlist_t *playlist);
xmms_medialib_entry_t xmms_playlist_current_entry (xmms_playlist_t *playlist);
gboolean xmms_playlist_add_url (xmms_playlist_t *playlist, gchar *plname, gchar *nurl, xmms_error_t *err);
gboolean xmms_playlist_add_idlist (xmms_playlist_t *playlist, gchar *plname, xmmsv_coll_t *coll, xmms_error_t *err);
gboolean xmms_playlist_add_collection (xmms_playlist_t *playlist, gchar *plname, xmmsv_coll_t *coll, GList *order, xmms_error_t *err);
void xmms_playlist_add_entry_unlocked (xmms_playlist_t *playlist, const gchar *plname, xmmsv_coll_t *plcoll, xmms_medialib_entry_t file, xmms_error_t *err);
GList * xmms_playlist_list (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *err);
GTree * xmms_playlist_current_pos (xmms_playlist_t *playlist, gchar *plname, xmms_error_t *err);
const gchar * xmms_playlist_current_active (xmms_playlist_t *playlist, xmms_error_t *err);
guint xmms_playlist_set_current_position (xmms_playlist_t *playlist, guint32 pos, xmms_error_t *error);
gboolean xmms_playlist_remove_by_entry (xmms_playlist_t *playlist, xmms_medialib_entry_t entry);

void xmms_playlist_add_entry (xmms_playlist_t *playlist, gchar *plname, xmms_medialib_entry_t file, xmms_error_t *err);
void xmms_playlist_insert_entry (xmms_playlist_t *playlist, gchar *plname, guint32 pos, xmms_medialib_entry_t file, xmms_error_t *err);

xmms_mediainfo_reader_t *xmms_playlist_mediainfo_reader_get (xmms_playlist_t *playlist);


GTree *xmms_playlist_changed_msg_new (xmms_playlist_t *playlist, xmms_playlist_changed_actions_t type, guint32 id, const gchar *plname);
void xmms_playlist_changed_msg_send (xmms_playlist_t *playlist, GTree *dict);

#define XMMS_PLAYLIST_COLLECTION_CHANGED_MSG(playlist, name) xmms_playlist_changed_msg_send (playlist, xmms_playlist_changed_msg_new (playlist, XMMS_PLAYLIST_CHANGED_UPDATE, 0, name))

/*
 * Entry modifications
 */


#endif
