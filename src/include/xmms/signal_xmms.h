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




#ifndef __SIGNAL_XMMS_H__
#define __SIGNAL_XMMS_H__

#define XMMS_OBJECT_PLAYBACK "/xmms/playback"
#define XMMS_OBJECT_PLAYLIST "/xmms/playlist"
#define XMMS_OBJECT_CLIENT "/xmms/client"
#define XMMS_OBJECT_CORE "/xmms/core"
#define XMMS_OBJECT_CONFIG "/xmms/config"
#define XMMS_OBJECT_TRANSPORT "/xmms/transport"
#define XMMS_OBJECT_VISUALISATION "/xmms/visualisation"

#define XMMS_DBUS_INTERFACE "org.xmms"

#define XMMS_METHOD_QUIT "quit"
#define XMMS_METHOD_NEXT "next"
#define XMMS_METHOD_PREV "prev"
#define XMMS_METHOD_PLAY "play"
#define XMMS_METHOD_STOP "stop"
#define XMMS_METHOD_PAUSE "pause"
#define XMMS_METHOD_STATUS "status"
#define XMMS_METHOD_CURRENTID "currentid"
#define XMMS_METHOD_SHUFFLE "shuffle"
#define XMMS_METHOD_SEEKMS "seekms"
#define XMMS_METHOD_SEEKSAMPLES "seeksamples"
#define XMMS_METHOD_SAVE "save"
#define XMMS_METHOD_SORT "sort"
#define XMMS_METHOD_JUMP "jump"
#define XMMS_METHOD_ADD "add"
#define XMMS_METHOD_LIST "list"
#define XMMS_METHOD_REMOVE "remove"
#define XMMS_METHOD_MOVE "move"
#define XMMS_METHOD_CLEAR "clear"
#define XMMS_METHOD_GETMEDIAINFO "getmediainfo"
#define XMMS_METHOD_MEDIAINFO "mediainfo"
#define XMMS_METHOD_MEDIAINFO_ID "mediainfoid"
#define XMMS_METHOD_SETMODE "setmode"
#define XMMS_METHOD_SETVALUE "setvalue"
#define XMMS_METHOD_REGISTER "register"
#define XMMS_METHOD_UNREGISTER "unregister"
#define XMMS_METHOD_PLAYTIME "playtime"
#define XMMS_METHOD_INFORMATION "information"
#define XMMS_METHOD_SPECTRUM "spectrum"


/* Playback msgs */
/*
#define XMMS_SIGNAL_PLAYBACK_PLAY "org.xmms.playback.play"
#define XMMS_SIGNAL_PLAYBACK_PAUSE "org.xmms.playback.pause"
#define XMMS_SIGNAL_PLAYBACK_NEXT "org.xmms.playback.next"
#define XMMS_SIGNAL_PLAYBACK_PREV "org.xmms.playback.prev"
#define XMMS_SIGNAL_PLAYBACK_SEEK_MS "org.xmms.playback.seek.ms"
#define XMMS_SIGNAL_PLAYBACK_SEEK_SAMPLES "org.xmms.playback.seek.samples"
#define XMMS_SIGNAL_PLAYBACK_STOP "/xmms/playback::stop"
*/
#define XMMS_SIGNAL_PLAYBACK_STATUS "/xmms/playback::status"
#define XMMS_SIGNAL_PLAYBACK_CURRENTID "/xmms/playback::currentid"
#define XMMS_SIGNAL_PLAYBACK_PLAYTIME "/xmms/playback::playtime"

/* Playlist msgs */
#define XMMS_SIGNAL_PLAYLIST_LIST "/xmms/playlist::list"
#define XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID "/xmms/playlist::mediainfoid"
#define XMMS_SIGNAL_PLAYLIST_CHANGED "/xmms/playlist::change"
#define XMMS_SIGNAL_PLAYLIST_MEDIAINFO "/xmms/playlist::mediainfo"
#define XMMS_SIGNAL_PLAYLIST_ADD "/xmms/playlist::add"
#define XMMS_SIGNAL_PLAYLIST_REMOVE "/xmms/playlist::remove"
#define XMMS_SIGNAL_PLAYLIST_SHUFFLE "/xmms/playlist::shuffle"
#define XMMS_SIGNAL_PLAYLIST_CLEAR "/xmms/playlist::clear"
#define XMMS_SIGNAL_PLAYLIST_JUMP "/xmms/playlist::jump"
#define XMMS_SIGNAL_PLAYLIST_MOVE "/xmms/playlist::move"
#define XMMS_SIGNAL_PLAYLIST_SORT "/xmms/playlist::sort"

/*
#define XMMS_SIGNAL_PLAYLIST_SAVE "org.xmms.playlist.save"
#define XMMS_SIGNAL_PLAYLIST_MODE_SET "org.xmms.playlist.mode.set"
*/
/* Core msgs */

#define XMMS_SIGNAL_CORE_QUIT "/xmms/core::quit"
#define XMMS_SIGNAL_CORE_DISCONNECT "/org/freedesktop/Local::Disconnected"
#define XMMS_SIGNAL_CORE_INFORMATION "/xmms/core::information"
#define XMMS_SIGNAL_CORE_SIGNAL_REGISTER "/xmms/core::register"
#define XMMS_SIGNAL_CORE_SIGNAL_UNREGISTER "/xmms/core::unregister"

/* Transport msgs */
#define XMMS_SIGNAL_TRANSPORT_MIMETYPE "/xmms/transport::mimetype"
#define XMMS_SIGNAL_TRANSPORT_LIST "/xmms/transport::list"

/* Output msgs */
#define XMMS_SIGNAL_OUTPUT_EOS_REACHED "/xmms/output::eos_reached"
#define XMMS_SIGNAL_OUTPUT_OPEN_FAIL "/xmms/output::open_fail"

/* Visualisation msgs */
#define XMMS_SIGNAL_VISUALISATION_SPECTRUM "/xmms/visualisation::spectrum"

/* Config msgs */
#define XMMS_SIGNAL_CONFIG_SAVE "/xmms/config::save"
#define XMMS_SIGNAL_CONFIG_VALUE_CHANGE "/xmms/config::value_change"

#endif /* __SIGNAL_XMMS_H__ */
