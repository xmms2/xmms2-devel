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

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;

import org.xmms2.xmms2bindings.SWIGTYPE_p_xmmsc_connection_St;
import org.xmms2.xmms2bindings.SWIGTYPE_p_xmmsc_result_St;
import org.xmms2.xmms2bindings.Xmmsclient;
import org.xmms2.xmms2bindings.XmmsclientConstants;
import org.xmms2.xmms2bindings.xmmsc_result_type_t;

/**
 * User internally only by org.xmms2.Xmms2
 */

final class Xmms2Backoffice implements CallbacksListener {
    private Method errorOccured, medialibSelect, configvalChanged,
            playtimeSignal, visdataSignal, mediareaderSignal,
            connectionEstablished, playbackStatusChanged,
            playbackVolumeChanged, playlistChanged,
            playlistCurrentPositionChanged, titleChanged, mediareaderStatus,
            medialibEntryChanged, medialibEntryAdded, medialibCurrentID,
            medialibPlaylistLoaded, pluginList;

    private Xmms2 myFront;

    protected SWIGTYPE_p_xmmsc_connection_St connectionOne, connectionTwo;

    private JMain main;

    protected boolean connected = false;

    private HashMap dictForeachMap;

    private Title workingTitle;

    private boolean broadcastsEnabled = false;

    protected Xmms2Backoffice(Xmms2 myFront, String clientname)
            throws Xmms2Exception {
        this.myFront = myFront;
        connectionOne = Xmmsclient.xmmsc_init(clientname + "1");
        connectionTwo = Xmmsclient.xmmsc_init(clientname + "2");
        if (connectionOne == null) {
            throw new Xmms2Exception(
                    "Initializing Xmms2-Connection #1 didn't succeed!\n"
                            + "Message from xmms2: "
                            + Xmmsclient.xmmsc_get_last_error(connectionOne));
        }
        if (connectionTwo == null) {
            throw new Xmms2Exception(
                    "Initializing Xmms2-Connection #2 didn't succeed!\n"
                            + "Message from xmms2: "
                            + Xmmsclient.xmmsc_get_last_error(connectionTwo));
        }
        initMethods();
        main = new JMain(this, connectionOne);
    }

    protected void connect() throws Xmms2Exception {
        if (!connected)
            callbackDisconnect(0);
        if (!connected)
            throw new Xmms2Exception("Problems connecting to xmms2d\n"
                    + "Message from xmms2 connection #1: "
                    + Xmmsclient.xmmsc_get_last_error(connectionOne)
                    + "\nMessage from xmms2 connection #2: "
                    + Xmmsclient.xmmsc_get_last_error(connectionTwo));
        if (connected)
            main.start();
    }

    protected void enableBroadcasts() {
        broadcastsEnabled = true;
        initLoop();
    }

    protected void spinDown() {
        Xmmsclient.xmmsc_disconnect_callback_set(connectionOne, null, 0);
        Xmmsclient.xmmsc_disconnect_callback_set(connectionTwo, null, 0);
        if (main != null)
            main.spinDown();
        Xmmsclient.xmmsc_unref(connectionOne);
        Xmmsclient.xmmsc_unref(connectionTwo);
    }

    private void initLoop() {
        main.setConfigvalChangedCallback(-1);
        main.setMedialibEntryChangedCallback(-1);
        main.setMedialibPlaylistLoadedCallback(-1);
        main.setPlaybackCurrentIDCallback(-1);
        main.setPlaybackStatusCallback(-1);
        main.setPlaylistChangedCallback(-1);
        main.setPlaylistCurrentPositionCallback(-1);
        main.setPlaybackPlaytimeSignal(-1);
        main.setVisualizationDataSignal(-1);
        main.setMediareaderUnindexedSignal(-1);
        main.setPlaybackVolumeChangedCallback(-1);
        main.setMediainfoReaderStatusCallback(-1);
    }

    private void initMethods() {
        try {
            errorOccured = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2ErrorOccured", new Class[] { Xmms2Event.class });
            medialibSelect = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2MedialibSelect", new Class[] { Xmms2Event.class });
            configvalChanged = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2ConfigvalChanged", new Class[] { Xmms2Event.class });
            playtimeSignal = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2PlaytimeSignal", new Class[] { Xmms2Event.class });
            visdataSignal = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2VisualisationdataSignal",
                    new Class[] { Xmms2Event.class });
            mediareaderSignal = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2MediareaderSignal", new Class[] { Xmms2Event.class });
            connectionEstablished = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2ConnectionEstablished",
                    new Class[] { Xmms2Event.class });
            playbackStatusChanged = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2PlaybackStatusChanged",
                    new Class[] { Xmms2Event.class });
            playbackVolumeChanged = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2PlaybackVolumeChanged",
                    new Class[] { Xmms2Event.class });
            playlistChanged = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2PlaylistChanged", new Class[] { Xmms2Event.class });
            playlistCurrentPositionChanged = Xmms2Listener.class
                    .getDeclaredMethod("xmms2PlaylistCurrentPositionChanged",
                            new Class[] { Xmms2Event.class });
            titleChanged = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2TitleChanged", new Class[] { Xmms2Event.class });
            mediareaderStatus = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2MediareaderStatusChanged",
                    new Class[] { Xmms2Event.class });
            medialibEntryChanged = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2MedialibEntryChanged",
                    new Class[] { Xmms2Event.class });
            medialibEntryAdded = Xmms2Listener.class
                    .getDeclaredMethod("xmms2MedialibEntryAdded",
                            new Class[] { Xmms2Event.class });
            medialibCurrentID = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2MedialibCurrentID", new Class[] { Xmms2Event.class });
            medialibPlaylistLoaded = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2MedialibPlaylistLoaded",
                    new Class[] { Xmms2Event.class });
            pluginList = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2PluginList", new Class[] { Xmms2Event.class });
        } catch (Throwable e) {
            System.err.println("problems when initializing methods");
        }
    }

    protected void lockDictForeach() {
        while (dictForeachMap != null)
            try {
                Thread.sleep(20);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        dictForeachMap = new HashMap();
    }

    protected Map unlockDictForeach() {
        HashMap tmp = dictForeachMap;
        dictForeachMap = null;
        return tmp;
    }

    protected void lockTitle() {
		if (workingTitle != null)
			synchronized (workingTitle){
				try {
					workingTitle.wait();
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}
		workingTitle = new Title();
    }

    protected Title unlockTitle() {
    	Title t = null;
    	synchronized (workingTitle){
    		t = workingTitle;
    		workingTitle.notify();
    		workingTitle = null;
    	}
        return t;
    }

    public void callbackConfigvalChanged(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        lockDictForeach();
        Xmmsclient.xmmsc_result_dict_foreach(result,
                XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 0);
        Map map = unlockDictForeach();
        myFront.notifiyListeners(configvalChanged, new Xmms2Event(user_data,
                Xmms2Listener.MAP_TYPE, map));
    }

    public void callbackPlaybackStatus(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long state[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, state);
        myFront.notifiyListeners(playbackStatusChanged, new Xmms2Event(
                user_data, Xmms2Listener.LONG_TYPE, new Long(state[0])));
    }

    public void callbackPlaybackID(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long id[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, id);
        myFront.notifiyListeners(medialibCurrentID, new Xmms2Event(user_data,
                Xmms2Listener.LONG_TYPE, new Long(id[0])));
    }

    public void callbackPlaylistChanged(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }

        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
            ArrayList pl = new ArrayList();
            Xmmsclient.xmmsc_result_list_first(result);
            long id[] = new long[1];
            while (Xmmsclient.xmmsc_result_list_valid(result) == 1) {
                Xmmsclient.xmmsc_result_get_uint(result, id);
                pl.add(new Long(id[0]));
                Xmmsclient.xmmsc_result_list_next(result);
            }
            myFront.notifiyListeners(playlistChanged, new Xmms2Event(user_data,
                    Xmms2Listener.LIST_TYPE, pl));
            if (pl.size() > 0) {
                SWIGTYPE_p_xmmsc_result_St result2 = Xmmsclient
                        .xmmsc_playlist_current_pos(connectionOne);
                Xmmsclient.xmmsc_result_notifier_set(result2,
                        XmmsclientConstants.CALLBACK_PLAYLIST_CURRENT_POSITION,
                        -1);
                Xmmsclient.xmmsc_result_unref(result2);
            }
        } else {
            SWIGTYPE_p_xmmsc_result_St result2 = Xmmsclient
                    .xmmsc_playlist_list(connectionOne);
            Xmmsclient.xmmsc_result_notifier_set(result2,
                    XmmsclientConstants.CALLBACK_PLAYLIST_CHANGED, user_data);
            Xmmsclient.xmmsc_result_unref(result2);
        }
    }

    public void callbackMedialibEntryChanged(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long id[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, id);
        myFront.notifiyListeners(medialibEntryChanged, new Xmms2Event(
                user_data, Xmms2Listener.LONG_TYPE, new Long(id[0])));
    }

    public void callbackMedialibEntryAdded(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long id[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, id);
        myFront.notifiyListeners(medialibEntryAdded, new Xmms2Event(user_data,
                Xmms2Listener.LONG_TYPE, new Long(id[0])));
    }

    public void callbackMediainfoReaderStatus(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        int state[] = new int[1];
        Xmmsclient.xmmsc_result_get_int(result, state);
        myFront.notifiyListeners(mediareaderStatus, new Xmms2Event(user_data,
                Xmms2Listener.LONG_TYPE, new Long(state[0])));
    }

    public void callbackMedialibPlaylistLoaded(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
            Xmmsclient.xmmsc_result_list_first(result);
            List entries = new LinkedList();
            while (Xmmsclient.xmmsc_result_list_valid(result) == 1) {
                String name[] = new String[1];
                Xmmsclient.xmmsc_result_get_string(result, name);
                entries.add(name[0]);
                Xmmsclient.xmmsc_result_list_next(result);
            }
            myFront.notifiyListeners(medialibPlaylistLoaded, new Xmms2Event(
                    user_data, Xmms2Listener.LIST_TYPE, entries));
        } else {
            myFront.notifiyListeners(medialibPlaylistLoaded, new Xmms2Event(
                    user_data, Xmms2Listener.VOID_TYPE, null));
        }
    }

    public void callbackPlaylistCurrentPosition(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long[] pos = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, pos);
        myFront.notifiyListeners(playlistCurrentPositionChanged,
                new Xmms2Event(user_data, Xmms2Listener.LONG_TYPE, new Long(
                        pos[0])));
    }

    public void callbackPlaybackVolumeChanged(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        lockDictForeach();
        Xmmsclient.xmmsc_result_dict_foreach(result,
                XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 0);
        Map vol = unlockDictForeach();
        myFront.notifiyListeners(playbackVolumeChanged, new Xmms2Event(
                user_data, Xmms2Listener.MAP_TYPE, vol));
    }

    public void callbackDisconnect(int error) {
        String path = null;
        if (myFront.serverip.equals("") && myFront.serverport.equals(""))
            path = System.getProperty("XMMS_PATH");
        else
            path = "tcp://" + myFront.serverip + ":" + myFront.serverport;
        if (path != null
                && (path.equals("") || path.equals("${env.XMMS_PATH}")))
            path = null;

        if (Xmmsclient.xmmsc_connect(connectionOne, path) <= 0) {
            connected = false;
            myFront.notifiyListeners(errorOccured, new Xmms2Event(-1,
                    Xmms2Listener.ERROR_TYPE, Xmmsclient
                            .xmmsc_get_last_error(connectionOne)));
        } else if (Xmmsclient.xmmsc_connect(connectionTwo, path) <= 0) {
            connected = false;
            myFront.notifiyListeners(errorOccured, new Xmms2Event(-1,
                    Xmms2Listener.ERROR_TYPE, Xmmsclient
                            .xmmsc_get_last_error(connectionTwo)));
        } else {
            connected = true;
            if (broadcastsEnabled)
                initLoop();
            Xmmsclient.xmmsc_disconnect_callback_set(connectionOne,
                    XmmsclientConstants.DISCONNECT_CALLBACK, 0);
            Xmmsclient.xmmsc_disconnect_callback_set(connectionTwo,
                    XmmsclientConstants.DISCONNECT_CALLBACK, 0);
        }
        myFront.notifiyListeners(connectionEstablished, new Xmms2Event(-1,
                Xmms2Listener.BOOL_TYPE, new Boolean(connected)));

    }

    public void lockFunction(int user_data) {
    }

    public void unlockFunction(int user_data) {
    }

    public void callbackDictForeachFunction(String key, int type, String value,
            int user_data) {
        if (user_data == 0 && dictForeachMap != null) {
            if (key != null && !key.equals("")) {
            	try {
            		if (key.equals("id"))
            			workingTitle.setID(Long.parseLong(value));
            		else
            			dictForeachMap.put(key, value);
            	} catch (Exception e){
            		e.printStackTrace();
            	}
            }
        }
    }

    public void callbackPropdictForeachFunction(String key, int type,
            String value, String source, int user_data) {
        if (user_data == 0 && workingTitle != null) {
            if (key != null && !key.equals("")) {
            	try {
            		if (key.equals("id"))
            			workingTitle.setID(Long.parseLong(value));
            		else
            			workingTitle.setAttribute(key, value);
            	} catch (Exception e){
            		e.printStackTrace();
            	}
            }
        }
        if (user_data == 1 && dictForeachMap != null) {
            if (key != null && !key.equals("")) {
                dictForeachMap.put(key, value);
            }
        }
    }

    public void signalPlaybackPlaytime(long res, int user_data) {
        final SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_SIGNAL.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long playtime[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, playtime);
        myFront.notifiyListeners(playtimeSignal, new Xmms2Event(-1,
                Xmms2Listener.LONG_TYPE, new Long(playtime[0])));

        new Timer().schedule(new TimerTask() {
            public void run() {
                SWIGTYPE_p_xmmsc_result_St res2 = Xmmsclient
                        .xmmsc_result_restart(result);
                Xmmsclient.xmmsc_result_unref(result);
                Xmmsclient.xmmsc_result_unref(res2);
            }
        }, 100);
    }

    public void signalVisualizationData(long res, int user_data) {
        // TODO
    }

    public void signalMediareaderUnindexed(long res, int user_data) {
        final SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_SIGNAL.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long todo[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, todo);
        myFront.notifiyListeners(mediareaderSignal, new Xmms2Event(-1,
                Xmms2Listener.LONG_TYPE, new Long(todo[0])));

        SWIGTYPE_p_xmmsc_result_St res2 = Xmmsclient
                .xmmsc_result_restart(result);
        Xmmsclient.xmmsc_result_unref(result);
        Xmmsclient.xmmsc_result_unref(res2);
    }

    /**
     * Handles mlibSelect and mlibPlaylistsList
     */
    public void userDefinedCallback1(long res, int user_data) {
        final SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
            Xmmsclient.xmmsc_result_list_first(result);
            List entries = new LinkedList();
            while (Xmmsclient.xmmsc_result_list_valid(result) == 1) {
                lockDictForeach();
                Xmmsclient.xmmsc_result_dict_foreach(result,
                        XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 0);
                entries.add(unlockDictForeach());
                Xmmsclient.xmmsc_result_list_next(result);
            }
            myFront.notifiyListeners(medialibSelect, new Xmms2Event(user_data,
                    Xmms2Listener.LIST_TYPE, entries));
        } else {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
        }
    }

    /**
     * Handles getTitle() from org.xmms2.Xmms2
     */
    public void userDefinedCallback2(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        lockTitle();
        if (myFront.sourcePref != null)
            Xmmsclient.xmmsc_result_source_preference_set(result,
                    myFront.sourcePref);
        Xmmsclient.xmmsc_result_propdict_foreach(result,
                XmmsclientConstants.CALLBACK_PROPDICT_FOREACH_FUNCTION, 0);
        Title t = unlockTitle();
        myFront.notifiyListeners(titleChanged, new Xmms2Event(user_data,
                Xmms2Listener.TITLE_TYPE, t));
    }

    /**
     * Handles pluginsList
     */
    public void userDefinedCallback3(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        if (xmmsc_result_type_t.XMMSC_RESULT_CLASS_BROADCAST.equals(Xmmsclient
                .xmmsc_result_get_class(result)))
            user_data = -1;
        else
            user_data = 1;
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
            HashMap map = new HashMap();
            Xmmsclient.xmmsc_result_list_first(result);
            String shortDes[] = new String[1];
            String longDes[] = new String[1];
            while (Xmmsclient.xmmsc_result_list_valid(result) == 1) {
                if (Xmmsclient.xmmsc_result_get_dict_entry_str(result,
                        "shortname", shortDes) == 1) {
                    Xmmsclient.xmmsc_result_get_dict_entry_str(result,
                            "shortname", longDes);
                    map.put(shortDes[0], longDes[0]);
                }
                Xmmsclient.xmmsc_result_list_next(result);
            }
            myFront.notifiyListeners(pluginList, new Xmms2Event(user_data,
                    Xmms2Listener.MAP_TYPE, map));
        }
    }

    protected String isError(SWIGTYPE_p_xmmsc_result_St result) {
        if (Xmmsclient.xmmsc_result_iserror(result) == 1) {
            return Xmmsclient.xmmsc_result_get_error(result);
        }
        return "";
    }
}
