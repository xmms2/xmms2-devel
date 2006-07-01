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
import java.util.LinkedList;
import java.util.List;

import org.xmms2.events.Xmms2Event;
import org.xmms2.events.Xmms2Listener;
import org.xmms2.events.Xmms2PlaylistEvent;
import org.xmms2.events.Xmms2PlaylistPositionEvent;
import org.xmms2.xmms2bindings.SWIGTYPE_p_xmmsc_connection_St;
import org.xmms2.xmms2bindings.SWIGTYPE_p_xmmsc_result_St;
import org.xmms2.xmms2bindings.Xmmsclient;
import org.xmms2.xmms2bindings.XmmsclientConstants;

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
            medialibPlaylistLoaded, pluginList, miscEvent;

    private Xmms2 myFront;

    protected SWIGTYPE_p_xmmsc_connection_St connectionOne, connectionTwo;

    private JMain main;

    protected boolean connected = false;

    private Dict dictForeachMap;
    
    private PropDict propDictForeachMap;

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
        Xmmsclient.xmmsc_disconnect_callback_set(connectionOne, null, 
        		Xmmsclient.convertIntToVoidP(0));
        Xmmsclient.xmmsc_disconnect_callback_set(connectionTwo, null, 
        		Xmmsclient.convertIntToVoidP(0));
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
                    "xmms2PlaylistChanged", new Class[] { Xmms2PlaylistEvent.class });
            playlistCurrentPositionChanged = Xmms2Listener.class
                    .getDeclaredMethod("xmms2PlaylistCurrentPositionChanged",
                            new Class[] { Xmms2PlaylistPositionEvent.class });
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
            miscEvent = Xmms2Listener.class.getDeclaredMethod(
                    "xmms2MiscEvent", new Class[] { Xmms2Event.class });
        } catch (Throwable e) {
            System.err.println("problems initializing methods");
        }
    }

    protected void lockDictForeach() {
        while (dictForeachMap != null)
            try {
                Thread.sleep(20);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        dictForeachMap = new Dict();
    }

    protected Dict unlockDictForeach() {
        Dict tmp = dictForeachMap;
        dictForeachMap = null;
        return tmp;
    }
    
    protected void lockPropDictForeach() {
        while (propDictForeachMap != null)
            try {
                Thread.sleep(20);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        propDictForeachMap = new PropDict();
    }

    protected PropDict unlockPropDictForeach() {
        PropDict tmp = propDictForeachMap;
        propDictForeachMap = null;
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
        
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        lockDictForeach();
        Xmmsclient.xmmsc_result_dict_foreach(result,
                XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 
                Xmmsclient.convertIntToVoidP(0));
        Dict map = unlockDictForeach();
        myFront.notifiyListeners(configvalChanged, new Xmms2Event(user_data,
                Xmms2Listener.DICT_TYPE, map));
    }

    public void callbackPlaybackStatus(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        
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
            myFront.pl.updateList(pl);
            myFront.notifiyListeners(playlistChanged, new Xmms2PlaylistEvent(user_data, 
                                     myFront.pl));
            if (pl.size() > 0) {
                SWIGTYPE_p_xmmsc_result_St result2 = Xmmsclient
                        .xmmsc_playlist_current_pos(connectionOne);
                Xmmsclient.xmmsc_result_notifier_set(result2,
                        XmmsclientConstants.CALLBACK_PLAYLIST_CURRENT_POSITION,
                        Xmmsclient.convertIntToVoidP(-1));
                Xmmsclient.xmmsc_result_unref(result2);
            }
        } else {
            SWIGTYPE_p_xmmsc_result_St result2 = Xmmsclient
                    .xmmsc_playlist_list(connectionOne);
            Xmmsclient.xmmsc_result_notifier_set(result2,
                    XmmsclientConstants.CALLBACK_PLAYLIST_CHANGED, 
                    Xmmsclient.convertIntToVoidP(user_data));
            Xmmsclient.xmmsc_result_unref(result2);
        }
    }

    public void callbackMedialibEntryChanged(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        
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
        
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long[] pos = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, pos);
        myFront.pl.updatePosition(pos[0]);
        myFront.notifiyListeners(playlistCurrentPositionChanged,
                new Xmms2PlaylistPositionEvent(user_data, new Long(pos[0])));
    }

    public void callbackPlaybackVolumeChanged(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        lockDictForeach();
        Xmmsclient.xmmsc_result_dict_foreach(result,
                XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 
                Xmmsclient.convertIntToVoidP(0));
        Dict vol = unlockDictForeach();
        myFront.notifiyListeners(playbackVolumeChanged, new Xmms2Event(
                user_data, Xmms2Listener.DICT_TYPE, vol));
    }

    public void callbackDisconnect(int error) {
    	if (System.getProperty("XMMS_PATH") != null)
    		myFront.ipcPath = System.getProperty("XMMS_PATH");
        if (Xmmsclient.xmmsc_connect(connectionOne, myFront.ipcPath) <= 0) {
            connected = false;
            myFront.notifiyListeners(errorOccured, new Xmms2Event(-1,
                    Xmms2Listener.ERROR_TYPE, Xmmsclient
                            .xmmsc_get_last_error(connectionOne)));
        } else if (Xmmsclient.xmmsc_connect(connectionTwo, myFront.ipcPath) <= 0) {
            connected = false;
            myFront.notifiyListeners(errorOccured, new Xmms2Event(-1,
                    Xmms2Listener.ERROR_TYPE, Xmmsclient
                            .xmmsc_get_last_error(connectionTwo)));
        } else {
            connected = true;
            if (broadcastsEnabled)
                initLoop();
            Xmmsclient.xmmsc_disconnect_callback_set(connectionOne,
                    XmmsclientConstants.DISCONNECT_CALLBACK, 
                    Xmmsclient.convertIntToVoidP(0));
            Xmmsclient.xmmsc_disconnect_callback_set(connectionTwo,
                    XmmsclientConstants.DISCONNECT_CALLBACK, 
                    Xmmsclient.convertIntToVoidP(0));
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
            		dictForeachMap.putDictEntry(key, value);
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
            			workingTitle.setAttribute(key, value, source);
            	} catch (Exception e){
            		e.printStackTrace();
            	}
            }
        }
        if (user_data == 1 && propDictForeachMap != null) {
            if (key != null && !key.equals("")) {
                propDictForeachMap.putPropDictEntry(key, value, source);
            }
        }
    }

    public void signalPlaybackPlaytime(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        long playtime[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, playtime);
        result = Xmmsclient.xmmsc_result_restart(result);
		Xmmsclient.xmmsc_result_unref(result);
        myFront.notifiyListeners(playtimeSignal, new Xmms2Event(-1,
                Xmms2Listener.LONG_TYPE, new Long(playtime[0])));
    }

    public void signalVisualizationData(long res, int user_data) {
        // TODO
    }

    public void signalMediareaderUnindexed(long res, int user_data) {
        final SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        
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
        
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
            Xmmsclient.xmmsc_result_list_first(result);
            List entries = new LinkedList();
            while (Xmmsclient.xmmsc_result_list_valid(result) == 1) {
                lockDictForeach();
                Xmmsclient.xmmsc_result_dict_foreach(result,
                        XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 
                        Xmmsclient.convertIntToVoidP(0));
                entries.add(unlockDictForeach());
                Xmmsclient.xmmsc_result_list_next(result);
            }
            myFront.notifiyListeners(medialibSelect, new Xmms2Event(user_data,
                    Xmms2Listener.LIST_TYPE, entries));
        } else {
        	myFront.notifiyListeners(miscEvent, new Xmms2Event(user_data,
                    Xmms2Listener.VOID_TYPE, null));
        }
    }

    /**
     * Handles getTitle() from org.xmms2.Xmms2
     */
    public void userDefinedCallback2(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        
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
                XmmsclientConstants.CALLBACK_PROPDICT_FOREACH_FUNCTION, 
                Xmmsclient.convertIntToVoidP(0));
        Title t = unlockTitle();
        if (myFront.pl.indicesOfID(t.getID()).size() > 0){
        	myFront.pl.updateTitle(t);
        }
        myFront.notifiyListeners(titleChanged, new Xmms2Event(user_data,
                Xmms2Listener.TITLE_TYPE, t));
    }

    /**
     * Handles pluginsList
     */
    public void userDefinedCallback3(long res, int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .getResultFromPointer(res);
        
        if (!isError(result).equals("")) {
            myFront.notifiyListeners(errorOccured, new Xmms2Event(user_data,
                    Xmms2Listener.ERROR_TYPE, isError(result)));
            return;
        }
        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
            Dict map = new Dict();
            Xmmsclient.xmmsc_result_list_first(result);
            String shortDes[] = new String[1];
            String longDes[] = new String[1];
            while (Xmmsclient.xmmsc_result_list_valid(result) == 1) {
                if (Xmmsclient.xmmsc_result_get_dict_entry_str(result,
                        "shortname", shortDes) == 1) {
                    Xmmsclient.xmmsc_result_get_dict_entry_str(result,
                            "shortname", longDes);
                    map.putDictEntry(shortDes[0], longDes[0]);
                }
                Xmmsclient.xmmsc_result_list_next(result);
            }
            myFront.notifiyListeners(pluginList, new Xmms2Event(user_data,
                    Xmms2Listener.DICT_TYPE, map));
        }
    }

    protected String isError(SWIGTYPE_p_xmmsc_result_St result) {
        if (Xmmsclient.xmmsc_result_iserror(result) == 1) {
            return Xmmsclient.xmmsc_result_get_error(result);
        }
        return "";
    }
}
