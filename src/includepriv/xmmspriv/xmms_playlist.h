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

/*
 * Private defintions
 */

#define XMMS_MAX_URI_LEN 1024
#define XMMS_MEDIA_DATA_LEN 1024


struct xmms_playlist_St;
typedef struct xmms_playlist_St xmms_playlist_t;

#include "xmms/xmms_error.h"
#include "xmms/xmms_medialib.h"
#include "xmmspriv/xmms_mediainfo.h"

/*
 * Public functions
 */

xmms_playlist_t * xmms_playlist_init (void);

gboolean xmms_playlist_add (xmms_playlist_t *playlist, xmms_medialib_entry_t file, xmms_error_t *error);
gboolean xmms_playlist_advance (xmms_playlist_t *playlist);
xmms_medialib_entry_t xmms_playlist_current_entry (xmms_playlist_t *playlist);
gboolean xmms_playlist_addurl (xmms_playlist_t *playlist, gchar *nurl, xmms_error_t *err);
GList * xmms_playlist_list (xmms_playlist_t *playlist, xmms_error_t *err);


xmms_mediainfo_reader_t *xmms_playlist_mediainfo_reader_get (xmms_playlist_t *playlist);

/*
 * Entry modifications
 */


#endif
