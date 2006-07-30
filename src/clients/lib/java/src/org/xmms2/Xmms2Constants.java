/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

package org.xmms2;

import org.xmms2.wrapper.xmms2bindings.xmms_mediainfo_reader_status_t;
import org.xmms2.wrapper.xmms2bindings.xmms_playback_status_t;
import org.xmms2.wrapper.xmms2bindings.xmms_playlist_changed_actions_t;
import org.xmms2.wrapper.xmms2bindings.xmms_plugin_type_t;

public interface Xmms2Constants {
	public static final int PLAYBACK_IDLE = xmms_playback_status_t.XMMS_PLAYBACK_STATUS_STOP.swigValue();
	public static final int PLAYBACK_PAUSED = xmms_playback_status_t.XMMS_PLAYBACK_STATUS_PAUSE.swigValue();
	public static final int PLAYBACK_PLAYING = xmms_playback_status_t.XMMS_PLAYBACK_STATUS_PLAY.swigValue();
	
	public static final int MEDIAINFO_READER_IDLE = 
		xmms_mediainfo_reader_status_t.XMMS_MEDIAINFO_READER_STATUS_IDLE.swigValue();
	public static final int MEDIAINFO_READER_WORKING = 
		xmms_mediainfo_reader_status_t.XMMS_MEDIAINFO_READER_STATUS_RUNNING.swigValue();
	
	public static final int PLUGIN_ALL = xmms_plugin_type_t.XMMS_PLUGIN_TYPE_ALL.swigValue();
	public static final int PLUGIN_OUTPUT = xmms_plugin_type_t.XMMS_PLUGIN_TYPE_OUTPUT.swigValue();
	public static final int PLUGIN_EFFECT = xmms_plugin_type_t.XMMS_PLUGIN_TYPE_EFFECT.swigValue();
	public static final int PLUGIN_PLAYLIST = xmms_plugin_type_t.XMMS_PLUGIN_TYPE_PLAYLIST.swigValue();
	public static final int PLUGIN_XFORM = xmms_plugin_type_t.XMMS_PLUGIN_TYPE_XFORM.swigValue();
	
	public static final int PLAYLIST_ADD = xmms_playlist_changed_actions_t.XMMS_PLAYLIST_CHANGED_ADD.swigValue();
	public static final int PLAYLIST_SORT = xmms_playlist_changed_actions_t.XMMS_PLAYLIST_CHANGED_SORT.swigValue();
	public static final int PLAYLIST_SHUFFLE = xmms_playlist_changed_actions_t.XMMS_PLAYLIST_CHANGED_SHUFFLE.swigValue();
	public static final int PLAYLIST_REMOVE = xmms_playlist_changed_actions_t.XMMS_PLAYLIST_CHANGED_REMOVE.swigValue();
	public static final int PLAYLIST_INSERT = xmms_playlist_changed_actions_t.XMMS_PLAYLIST_CHANGED_INSERT.swigValue();
	public static final int PLAYLIST_MOVE = xmms_playlist_changed_actions_t.XMMS_PLAYLIST_CHANGED_MOVE.swigValue();
	public static final int PLAYLIST_CLEAR = xmms_playlist_changed_actions_t.XMMS_PLAYLIST_CHANGED_CLEAR.swigValue();
}
