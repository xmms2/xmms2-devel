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

public interface Xmms2Listener {
    public static final String PLAYLIST_TYPE = "PLAYLIST";
    
    public static final String LIST_TYPE = "LIST";

    public static final String DICT_TYPE = "DICT";
    
    public static final String PROPDICT_TYPE = "PROPDICT";

    public static final String LONG_TYPE = "INT";

    public static final String TITLE_TYPE = "TITLE";

    public static final String DOUBLE_TYPE = "DOUBLE";

    public static final String STRING_TYPE = "STRING";

    public static final String VOID_TYPE = "VOID";

    public static final String ERROR_TYPE = "ERROR";

    public static final String BOOL_TYPE = "BOOLEAN";

    public void xmms2ConnectionEstablished(Xmms2Event ev);
    
    public void xmms2MiscEvent(Xmms2Event ev);

    public void xmms2ErrorOccured(Xmms2Event ev);

    public void xmms2MedialibSelect(Xmms2Event ev);

    public void xmms2ConfigvalChanged(Xmms2ConfigEvent ev);

    public void xmms2PlaybackStatusChanged(Xmms2Event ev);

    public void xmms2PlaybackVolumeChanged(Xmms2Event ev);

    public void xmms2PlaylistChanged(Xmms2PlaylistEvent ev);

    public void xmms2PlaylistCurrentPositionChanged(Xmms2PlaylistPositionEvent ev);

    public void xmms2TitleChanged(Xmms2TitleEvent ev);

    public void xmms2MediareaderStatusChanged(Xmms2Event ev);

    public void xmms2MedialibEntryChanged(Xmms2Event ev);

    public void xmms2MedialibEntryAdded(Xmms2Event ev);

    public void xmms2MedialibCurrentID(Xmms2Event ev);

    public void xmms2MedialibPlaylistLoaded(Xmms2Event ev);

    public void xmms2PluginList(Xmms2Event ev);

    public void xmms2PlaytimeSignal(Xmms2Event ev);

    public void xmms2VisualisationdataSignal(Xmms2Event ev);

    public void xmms2MediareaderSignal(Xmms2Event ev);

}
