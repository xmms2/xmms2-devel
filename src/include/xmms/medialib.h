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




#ifndef __XMMS_MEDIALIB_H__
#define __XMMS_MEDIALIB_H__


#include "xmms/plugin.h"
#include "xmms/playlist.h"
#include "xmms/output.h"
#include <glib.h>
#include <sqlite.h>

#define XMMS_MEDIALIB_ENTRY_PROPERTY_MIME "mime"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ID "id"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_URL "url"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST "artist"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM "album"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE "title"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR "date"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR "tracknr"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE "genre"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE "bitrate"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT "comment"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION "duration"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNEL "channel"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE "samplerate"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD "lmod"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED "resolved"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK "gain_track"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM "gain_album"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK "peak_track"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM "peak_album"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID "album_id"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID "artist_id"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID "track_id"

/*
 * Method defintions.
 */ 

/** Callback to the query function. */
typedef int (*xmms_medialib_row_method_t) (void *pArg, int argc, char **argv, char **columnName);

typedef struct xmms_medialib_St xmms_medialib_t;

typedef guint32 xmms_medialib_entry_t;

/*
 * Public functions
 */
gboolean xmms_medialib_init (xmms_playlist_t *playlist);
gchar *xmms_medialib_entry_property_get (xmms_medialib_entry_t entry, const gchar *property);
guint xmms_medialib_entry_property_get_int (xmms_medialib_entry_t entry, const gchar *property);
gboolean xmms_medialib_entry_property_set (xmms_medialib_entry_t entry, const gchar *property, const gchar *value);
void xmms_medialib_entry_send_update (xmms_medialib_entry_t entry);
xmms_medialib_entry_t xmms_medialib_entry_new (const char *url);
GHashTable *xmms_medialib_entry_to_hashtable (xmms_medialib_entry_t entry);
guint xmms_medialib_entry_id_get (xmms_medialib_entry_t entry);
gboolean xmms_medialib_entry_is_resolved (xmms_medialib_entry_t entry);

sqlite *xmms_sqlite_open ();
gboolean xmms_sqlite_query (sqlite *sql, xmms_medialib_row_method_t method, void *udata, char *query, ...);
void xmms_sqlite_close ();

#endif /* __XMMS_MEDIALIB_H__ */
