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


#include <glib.h>
#include <xmms/xmms_object.h>

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
/** Indicates that this album is a compilation */
#define XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION "compilation"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID "album_id"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID "artist_id"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID "track_id"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ADDED "added"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_BPM "bpm"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_LASTSTARTED "laststarted"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_DECODER "decoder"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_TRANSPORT "transport"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_SAMPLEFMT_OUT "samplefmt:out"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_SAMPLEFMT_IN "samplefmt:in"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_SAMPLERATE_OUT "samplerate:out"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_SAMPLERATE_IN "samplerate:in"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_CHANNELS_OUT "channels:out"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_FMT_CHANNELS_IN "channels:in"


typedef guint32 xmms_medialib_entry_t;
typedef struct xmms_medialib_session_St xmms_medialib_session_t;

xmms_medialib_entry_t xmms_medialib_entry_new (xmms_medialib_session_t *session, const char *url);
gboolean xmms_medialib_playlist_add (xmms_medialib_session_t *session, gint playlist_id, xmms_medialib_entry_t entry);

xmms_object_cmd_value_t *xmms_medialib_entry_property_get_cmd_value (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *property);
gchar *xmms_medialib_entry_property_get_str (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *property);
guint xmms_medialib_entry_property_get_int (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *property);
gboolean xmms_medialib_entry_property_set_str (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *property, const gchar *value);
gboolean xmms_medialib_entry_property_set_int (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *property, gint value);
void xmms_medialib_entry_send_update (xmms_medialib_entry_t entry);
guint32 xmms_medialib_get_random_entry (xmms_medialib_session_t *session);


#define xmms_medialib_begin() _xmms_medialib_begin(__FILE__, __LINE__)

xmms_medialib_session_t * _xmms_medialib_begin (const char *file, int line);
void xmms_medialib_end (xmms_medialib_session_t *session);
void xmms_medialib_session_source_set (xmms_medialib_session_t *session, gchar *source);

#endif /* __XMMS_MEDIALIB_H__ */
