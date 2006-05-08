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

/**
 * Extend this class if you don't want to have a huge bulk of empty methods in
 * your Callbacks-class. For descriptions to the methods have a look at
 * org.xmms2.CallbacksListener
 */

public abstract class CallbacksAdapter implements CallbacksListener {
    public void callbackConfigvalChanged(long res, int user_data) {
    }

    public void callbackPlaybackStatus(long res, int user_data) {
    }

    public void callbackPlaybackVolumeChanged(long res, int user_data) {
    }

    public void callbackPlaybackID(long res, int user_data) {
    }

    public void callbackPlaylistChanged(long res, int user_data) {
    }

    public void callbackMedialibEntryChanged(long res, int user_data) {
    }

    public void callbackMedialibEntryAdded(long res, int user_data) {
    }

    public void callbackMedialibPlaylistLoaded(long res, int user_data) {
    }

    public void callbackMediainfoReaderStatus(long res, int user_data) {
    }

    public void callbackPlaylistCurrentPosition(long res, int user_data) {
    }

    public void callbackDisconnect(int error) {
    }

    public void lockFunction(int user_data) {
    }

    public void unlockFunction(int user_data) {
    }

    public void callbackDictForeachFunction(String key, int type, String value,
            int user_data) {
    }

    public void callbackPropdictForeachFunction(String key, int type,
            String value, String source, int user_data) {
    }

    public void signalPlaybackPlaytime(long res, int user_data) {
    }

    public void signalVisualizationData(long res, int user_data) {
    }

    public void signalMediareaderUnindexed(long res, int user_data) {
    }

    public void userDefinedCallback1(long res, int user_data) {
    }

    public void userDefinedCallback2(long res, int user_data) {
    }

    public void userDefinedCallback3(long res, int user_data) {
    }
}
