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

package org.xmms2.wrapper;

/**
 * Interface CallbacksListener must be implemented by the class which should
 * handle the needed callbacks. This <b>doesn't</b> set the Callbacks already,
 * you have to set the callbacks in a way like this:
 * 
 * <br/><br/><i>org.xmms2.xmms2bindings.xmmsc_result_St result =
 * org.xmms2.xmms2bindings.Xmmsclient.xmmsc_broadcast_configval_changed(controlConnection);<br/>
 * org.xmms2.xmms2bindings.Xmmsclient.xmmsc_result_notifier_set(result,
 * org.xmms2.xmms2bindings.XmmsclientConstants.CALLBACK_CONFIGVAL_CHANGED,
 * null);</i><br/><br/>
 * 
 * You can get a result object out from the long parameter using
 * org.xmms2.xmms2bindings.Xmmsclient.get_result_from_pointer(). Don't unref the
 * gotten result unless you know what you are doing! You have to run
 * org.xmms2.SpecialJNI.setENV(CallbacksListener) first. Otherwise the callbacks
 * won't work.
 */

public interface CallbacksListener {
    /**
     * Callback which gets called when a configval changes. You can get the
     * changed configval with dict-entries name and value
     * 
     * @param res
     *            A pointer to xmmsc_result_t
     * @param user_data
     *            A user_data value
     */
    public void callbackConfigvalChanged(long res, int user_data);

    /**
     * Callback gets called when player's status changes. result holds a uint
     * 
     * @param res
     *            A pointer to xmmsc_result_t
     * @param user_data
     *            A user_data value
     */
    public void callbackPlaybackStatus(long res, int user_data);

    /**
     * Callback gets called when the playbackID changes. result holds a uint
     * 
     * @param res
     *            A pointer to xmmsc_result_t
     * @param user_data
     *            A user_data value
     */
    public void callbackPlaybackID(long res, int user_data);

    /**
     * Callback gets called when the playlist changes in some way
     * 
     * @param res
     *            A pointer to xmmsc_result_t
     * @param user_data
     *            A user_data value
     */
    public void callbackPlaylistChanged(long res, int user_data);

    /**
     * Callback gets called when an entry of the medialib changes in some way
     * 
     * @param res
     *            A pointer to xmmsc_result_t
     * @param user_data
     *            A user_data value
     */
    public void callbackMedialibEntryChanged(long res, int user_data);

    /**
     * Callback gets called when an entry is added to the medialib
     * 
     * @param res
     *            A pointer to xmmsc_result_t
     * @param user_data
     *            A user_data value
     */
    public void callbackMedialibEntryAdded(long res, int user_data);

    /**
     * indicates if the mediareader (medialib) is working or not. result holds
     * an int
     * 
     * @param res
     * @param user_data
     */
    public void callbackMediainfoReaderStatus(long res, int user_data);

    /**
     * Callback gets called when a playlist is loaded to the medialib
     * 
     * @param res
     *            A pointer to xmmsc_result_t
     * @param user_data
     *            A user_data value
     */
    public void callbackMedialibPlaylistLoaded(long res, int user_data);

    /**
     * Callback gets called when the current position in playlist changes
     * 
     * @param res
     *            A pointer to xmmsc_result_t
     * @param user_data
     *            A user_data value
     */
    public void callbackPlaylistCurrentPosition(long res, int user_data);

    /**
     * receives volume changed broadcasts
     * 
     * @param res
     * @param user_data
     */
    public void callbackPlaybackVolumeChanged(long res, int user_data);

    /**
     * Callback gets called when xmms2d disconnected.
     * 
     * @param error
     *            The error occured
     */
    public void callbackDisconnect(int error);

    /**
     * function which gets called when locking a connection
     * 
     * @param user_data
     */
    public void lockFunction(int user_data);

    /**
     * function which gets called when unlocking a connection
     * 
     * @param user_data
     */
    public void unlockFunction(int user_data);

    /**
     * Callback used to handle a foreach from xmmsc_result_t
     * 
     * @param key
     * @param type
     * @param value
     * @param user_data
     */
    public void callbackDictForeachFunction(String key, int type, String value,
            int user_data);

    /**
     * Callback for the foreach-with-source function of the xmmsclientlib
     * 
     * @param key
     * @param type
     * @param value
     * @param source
     * @param user_data
     */
    public void callbackPropdictForeachFunction(String key, int type,
            String value, String source, int user_data);

    /**
     * this signal will get called everytime playtime changes
     * 
     * @param res
     * @param user_data
     */
    public void signalPlaybackPlaytime(long res, int user_data);

    /**
     * signal is used for vis-data
     * 
     * @param res
     * @param user_data
     */
    public void signalVisualizationData(long res, int user_data);

    /**
     * signal is used to handle mediareader events - for only getting working
     * and idle signals use the broadcast instead
     * 
     * @param res
     * @param user_data
     */
    public void signalMediareaderUnindexed(long res, int user_data);

    /**
     * Some user defined callback. can be used as one likes to use it
     * 
     * @param res
     * @param user_data
     */
    public void userDefinedCallback1(long res, int user_data);

    /**
     * Some user defined callback. can be used as one likes to use it
     * 
     * @param res
     * @param user_data
     */
    public void userDefinedCallback2(long res, int user_data);

    /**
     * Some user defined callback. can be used as one likes to use it
     * 
     * @param res
     * @param user_data
     */
    public void userDefinedCallback3(long res, int user_data);
}
