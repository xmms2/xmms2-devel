/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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
#define XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT_LANG "commentlang"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION "duration"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNEL "channel"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNELS "channels"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLE_FMT "sample_format"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE "samplerate"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD "lmod"
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
#define XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE "size"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_IS_VBR "isvbr"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_SUBTUNES "subtunes"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_CHAIN "chain"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_TIMESPLAYED "timesplayed"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET "partofset"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT "picture_front"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_PICTURE_FRONT_MIME "picture_front_mime"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_STARTMS "startms"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_STOPMS "stopms"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS "status"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_DESCRIPTION "description"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_GROUPING "grouping"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_PERFORMER "performer"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_CONDUCTOR "conductor"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ARRANGER "arranger"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ORIGINAL_ARTIST "original_artist"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ARTIST "album_artist"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_PUBLISHER "publisher"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER "composer"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_ASIN "asin"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_COPYRIGHT "copyright"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_ARTIST "website_artist"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_FILE "website_file"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_PUBLISHER "website_publisher"
#define XMMS_MEDIALIB_ENTRY_PROPERTY_WEBSITE_COPYRIGHT "website_copyright"

G_BEGIN_DECLS

typedef gint32 xmms_medialib_entry_t;

G_END_DECLS

#endif /* __XMMS_MEDIALIB_H__ */
