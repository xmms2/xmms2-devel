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




#ifndef __XMMS_PLAYLIST_H__
#define __XMMS_PLAYLIST_H__

#include <glib.h>


/*
 * Public definitions
 */

typedef enum {
	XMMS_PLAYLIST_SET_NEXT_RELATIVE,
	XMMS_PLAYLIST_SET_NEXT_BYID,
} xmms_playlist_set_next_types_t;

typedef enum {
	XMMS_PLAYLIST_APPEND,
	XMMS_PLAYLIST_PREPEND,
} xmms_playlist_actions_t;

typedef enum {
	XMMS_PLAYLIST_CHANGED_ADD,
	XMMS_PLAYLIST_CHANGED_SHUFFLE,
	XMMS_PLAYLIST_CHANGED_REMOVE,
	XMMS_PLAYLIST_CHANGED_CLEAR,
	XMMS_PLAYLIST_CHANGED_MOVE,
	XMMS_PLAYLIST_CHANGED_SORT
} xmms_playlist_changed_actions_t;

/*
 * Private defintions
 */

#define XMMS_MAX_URI_LEN 1024
#define XMMS_MEDIA_DATA_LEN 1024


struct xmms_playlist_St;
typedef struct xmms_playlist_St xmms_playlist_t;

typedef struct xmms_playlist_changed_msg_St {
	gint type;
	guint id;
	guint arg;
} xmms_playlist_changed_msg_t;

#include "xmms/medialib.h"
#include "xmms/decoder.h"
#include "xmms/mediainfo.h"
#include "xmms/error.h"

/*
 * Public functions
 */

xmms_playlist_t * xmms_playlist_init (void);

gboolean xmms_playlist_add (xmms_playlist_t *playlist, xmms_medialib_entry_t file);
guint xmms_playlist_entries_total (xmms_playlist_t *playlist);
guint xmms_playlist_entries_left (xmms_playlist_t *playlist);
gint xmms_playlist_get_current_position (xmms_playlist_t *playlist);
gboolean xmms_playlist_advance (xmms_playlist_t *playlist);
xmms_medialib_entry_t xmms_playlist_current_entry (xmms_playlist_t *playlist);
gboolean xmms_playlist_id_remove (xmms_playlist_t *playlist, guint id, xmms_error_t *err);
xmms_decoder_t *xmms_playlist_next_start (xmms_playlist_t *playlist);
gboolean xmms_playlist_addurl (xmms_playlist_t *playlist, gchar *nurl, xmms_error_t *err);
void xmms_playlist_save (xmms_playlist_t *playlist, gchar *filename, xmms_error_t *err);
GList * xmms_playlist_list (xmms_playlist_t *playlist, xmms_error_t *err);

void xmms_playlist_wait (xmms_playlist_t *playlist);
GList *xmms_playlist_stats (xmms_playlist_t *playlist, GList *list);

xmms_mediainfo_thread_t *xmms_playlist_mediainfo_thread_get (xmms_playlist_t *playlist);

/*
 * Entry modifications
 */


#endif
