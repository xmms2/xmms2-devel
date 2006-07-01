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

package org.xmms2.events;

import org.xmms2.Playlist;

public class Xmms2PlaylistEvent extends Xmms2Event {
	public Xmms2PlaylistEvent(int tid, Playlist pl){
		super(tid, Xmms2Listener.PLAYLIST_TYPE, pl);
	}
	
	public Playlist getPlaylist(){
		return (Playlist)value;
	}
}
