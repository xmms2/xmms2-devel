/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <xmms/xmms_error.h>
#include <xmmspriv/xmms_medialib.h>
#include <xmmspriv/xmms_mediainfo.h>

/*
 * Public functions
 */

xmms_playlist_t * xmms_playlist_init (xmms_medialib_t *medialib, xmms_coll_dag_t *dag);

void xmms_playlist_update (xmms_playlist_t *playlist, const gchar *plname);

gboolean xmms_playlist_advance (xmms_playlist_t *playlist);
xmms_medialib_entry_t xmms_playlist_current_entry (xmms_playlist_t *playlist);
void xmms_playlist_add_entry_unlocked (xmms_playlist_t *playlist, const const gchar *plname, xmmsv_coll_t *plcoll, xmms_medialib_entry_t file, xmms_error_t *err);
GList * xmms_playlist_list (xmms_playlist_t *playlist, const gchar *plname, xmms_error_t *err);

void xmms_playlist_add_entry (xmms_playlist_t *playlist, const gchar *plname, xmms_medialib_entry_t file, xmms_error_t *err);
void xmms_playlist_insert_entry (xmms_playlist_t *playlist, const gchar *plname, guint32 pos, xmms_medialib_entry_t file, xmms_error_t *err);

/*
 * Entry modifications
 */


#endif
