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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;

import org.xmms2.events.*;
import org.xmms2.wrapper.xmms2bindings.SWIGTYPE_p_xmmsc_result_St;
import org.xmms2.wrapper.xmms2bindings.Xmmsclient;
import org.xmms2.wrapper.xmms2bindings.XmmsclientConstants;
import org.xmms2.wrapper.xmms2bindings.xmms_plugin_type_t;

/**
 * Use this class to work with Xmms2 in Java. call getInstance(), connect() and
 * then do whatever you want until you call spinDown(). But take care on errors
 * and set a Xmms2Listener to listen for events.
 * 
 * ExampleAsync: Xmms2 bleh = Xmms2.getInstance("YourClientName");
 * bleh.connect(); bleh.addXmms2Listener(new Xmms2Listener(){ ..... });
 * bleh.play(); # Wait some time or do some other weird things ;)
 * bleh.spinDown();
 * 
 * ExampleSync: Xmms2 bleh = Xmms2.getInstance("YourClientName");
 * bleh.connect(); Map configs = bleh.configvalListSync(); # Do some nifty
 * things with the configs you got # bleh.spinDown();
 * 
 * Don't use org.xmms2.xmms2bindings.* and org.xmms2.JMain
 * directly if you are using org.xmms2.Xmms2
 */

public final class Xmms2 {
    private static Xmms2 instance = null;

    protected String ipcPath = null;

    private ArrayList listeners;

    private Xmms2Backoffice xbo;

    protected String sourcePref[], source;
    
    protected Playlist pl;
    
    private int tid = 0;

    private Xmms2(String clientname) throws UnsatisfiedLinkError,
            Xmms2Exception {
        try {
            System.loadLibrary("xmms2java");
        } catch (UnsatisfiedLinkError e) {
            throw e;
        }

        xbo = new Xmms2Backoffice(this, clientname);
        listeners = new ArrayList();
    }
    
    private synchronized int getNextTID(){
    	return tid++;
    }

    public synchronized void addXmms2Listener(Xmms2Listener l) {
        listeners.add(l);
    }

    public synchronized void removeXmms2Listener(Xmms2Listener l) {
        if (listeners.contains(l))
            listeners.remove(l);
    }

    /**
     * Sourcepreference which is used to work with propdicts. Set null here to
     * use the default
     * 
     * @param pref
     */
    public void setSourcePreference(String pref[]) {
        sourcePref = pref;
    }

    /**
     * Source which is transmitted when editing entries. Set it to null or "" to
     * use the default
     * 
     * @param source
     */
    public void setSource(String source) {
        this.source = source;
    }

    /**
     * Call that function on startup _before_ issuing any commands
     * 
     * @throws Xmms2Exception
     * 
     */
    public void connect() throws Xmms2Exception {
        xbo.connect();
        pl = new Playlist(this);
    }

    /**
     * Enable the broadcasts
     */
    public void enableBroadcasts() {
        xbo.enableBroadcasts();
    }

    protected void notifiyListeners(Method method, Xmms2Event e) {
        for (ListIterator it = listeners.listIterator(); it.hasNext();) {
            try {
                method.invoke(it.next(), new Object[] { e });
            } catch (IllegalArgumentException e1) {
                e1.printStackTrace();
            } catch (IllegalAccessException e1) {
                e1.printStackTrace();
            } catch (InvocationTargetException e1) {
                e1.printStackTrace();
            }
        }
    }

    /**
     * Get an instance of Xmms2. Only the first instance gets clientname passed
     * 
     * @param clientname
     * @return Xmms2 instance
     * @throws UnsatisfiedLinkError
     *             if xmms2java cannot be found in java.library.path
     * @throws Xmms2Exception
     */
    public static Xmms2 getInstance(String clientname)
            throws UnsatisfiedLinkError, Xmms2Exception {
        if (instance == null)
            instance = new Xmms2(clientname);
        return instance;
    }

    public boolean isConnected() {
        return xbo.connected;
    }

    /**
     * Call that when you close your app to close the mainloop too
     */
    public void spinDown() {
        xbo.spinDown();
    }

    /**
     * Set serverip and serverport here. If both are set we accept them as is.
     * 
     * @param serverip
     * @param serverport
     */
    public void setConnectionParams(String serverip, String serverport) {
        setConnectionParams("tcp://" + serverip + ":" + serverport);
    }
    
    /**
     * Set ipcPath here. This setting overrides everything else
     * 
     * @param ipcPath
     */
    public void setConnectionParams(String ipcPath) {
        this.ipcPath = ipcPath;
    }
    
    /**
     * @return	The used path for the xmms2 client's configuration
     */
    public String getConfigurationPath(){
    	return Xmmsclient.xmmsc_userconfdir_get();
    }
    
    /**
     * @param input 
     * @return	Prepares some SQL-string for use with sqlite
     */
    public String getSQLPreparedString(String input){
    	return Xmmsclient.xmmsc_sqlite_prepare_string(input);
    }

    public short[] bindataBase64Decode(String data) {
    	short[] bindata = null;
    	long[] datalength = new long[1];
    	Xmmsclient.xmms_bindata_base64_decode_wrap(data, datalength, bindata);
    	return bindata;
    }
    
    public String bindataBase64Encode(short[] bindata) {
    	return Xmmsclient.xmms_bindata_base64_encode(bindata, bindata.length);
    }

    
    
    /*
     * Following void returning functions work almost as their c-pendants
     */
    private synchronized int handleNotifierTypePlayback(SWIGTYPE_p_xmmsc_result_St result) {
    	int t = getNextTID();
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.USER_DEFINED_CALLBACK_1, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int play() {
        return handleNotifierTypePlayback(Xmmsclient
                .xmmsc_playback_start(xbo.connectionOne));
    }

    public int stop() {
        return handleNotifierTypePlayback(Xmmsclient
                .xmmsc_playback_stop(xbo.connectionOne));
    }

    public int pause() {
        return handleNotifierTypePlayback(Xmmsclient
                .xmmsc_playback_pause(xbo.connectionOne));
    }
    
    public int tickle() {
        return handleNotifierTypePlayback(Xmmsclient
                .xmmsc_playback_tickle(xbo.connectionOne));
    }

    public int next() {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_set_next_rel(
                xbo.connectionOne, 1));
    }

    public int prev() {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_set_next_rel(
                xbo.connectionOne, -1));
    }

    protected int setNextRel(int x) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_set_next_rel(
                xbo.connectionOne, x));
    }

    protected int shuffle() {
        return handleNotifierTypePlayback(Xmmsclient
                .xmmsc_playlist_shuffle(xbo.connectionOne));
    }

    protected int clear() {
        return handleNotifierTypePlayback(Xmmsclient
                .xmmsc_playlist_clear(xbo.connectionOne));
    }

    protected int addId(long id) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_add_id(
                xbo.connectionOne, id));
    }

    protected int addUrl(String url) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_add(
                xbo.connectionOne, url));
    }

    protected int insertId(long id, int position) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_insert_id(
                xbo.connectionOne, position, id));
    }

    protected int insertUrl(String url, int position) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_insert(
                xbo.connectionOne, position, url));
    }

    protected int removeIndex(int index) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_remove(
                xbo.connectionOne, index));
    }

    protected int sort(String by) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_sort(
                xbo.connectionOne, by));
    }

    protected int setNextAbs(int index) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_set_next(
                xbo.connectionOne, index));
    }

    protected int move(int sourceIndex, int destIndex) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_move(
                xbo.connectionOne, sourceIndex, destIndex));
    }

    public int seek(int ms) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_seek_ms(
                xbo.connectionOne, ms));
    }

    public int seekRel(int ms) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_seek_ms_rel(
                xbo.connectionOne, ms));
    }

    public int seekSamples(int samples) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_seek_samples(
                xbo.connectionOne, samples));
    }

    public int seekSamplesRel(int samples) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_seek_samples_rel(
                xbo.connectionOne, samples));
    }

    public int configvalSet(String key, String val) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_configval_set(
                xbo.connectionOne, key, val));
    }

    public int configvalAdd(String key, String val) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_configval_register(
                xbo.connectionOne, key, val));
    }

    public int volumeSet(String channel, int value) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_volume_set(
                xbo.connectionOne, channel, value));
    }

    public int killXmms2d() {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_quit(xbo.connectionOne));
    }

    public int mlibAddUrl(String url) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_add_entry(
                xbo.connectionOne, url));
    }

    public int mlibAddToPlaylist(String sql) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_add_to_playlist(
                xbo.connectionOne, sql));
    }

    public int mlibRemoveId(int id) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_remove_entry(
                xbo.connectionOne, id));
    }

    public int mlibPropertySetStr(int id, String key, String value) {
        if (source == null || source.equals(""))
            return handleNotifierTypePlayback(Xmmsclient
                    .xmmsc_medialib_entry_property_set_str(xbo.connectionOne, id,
                            key, value));
        return handleNotifierTypePlayback(Xmmsclient
                    .xmmsc_medialib_entry_property_set_str_with_source(
                            xbo.connectionOne, id, source, key, value));
    }
    
    public int mlibPropertySetInt(int id, String key, int value) {
        if (source == null || source.equals(""))
            return handleNotifierTypePlayback(Xmmsclient
                    .xmmsc_medialib_entry_property_set_int(xbo.connectionOne, id,
                            key, value));
        return handleNotifierTypePlayback(Xmmsclient
                    .xmmsc_medialib_entry_property_set_int_with_source(
                            xbo.connectionOne, id, source, key, value));
    }

    public int mlibPropertyRemove(int id, String key) {
        if (source == null || source.equals(""))
            return handleNotifierTypePlayback(Xmmsclient
                    .xmmsc_medialib_entry_property_remove(xbo.connectionOne,
                            id, key));
        return handleNotifierTypePlayback(Xmmsclient
                    .xmmsc_medialib_entry_property_remove_with_source(
                            xbo.connectionOne, id, source, key));
    }

    public int saveCurrentPlaylist(String name) {
        return handleNotifierTypePlayback(Xmmsclient
                .xmmsc_medialib_playlist_save_current(xbo.connectionOne, name));
    }

    public int mlibImportPlaylist(String url, String name) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_import(
                xbo.connectionOne, name, url));
    }

    public int mlibExportPlaylist(String name, String mime) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_export(
                xbo.connectionOne, name, mime));
    }

    public int mlibLoadPlaylist(String name) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_load(
                xbo.connectionOne, name));
    }

    public int mlibRemovePlaylist(String name) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_remove(
                xbo.connectionOne, name));
    }

    public int mlibRehash(int id) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_rehash(
                xbo.connectionOne, id));
    }

    public int mlibPathImport(String url) {
        return handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_path_import(
                xbo.connectionOne, url));
    }

    /*
     * Following methods notify all Xmms2Listeners with Xmms2Events.
     */
    public int configvalListAsync() {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_configval_list(xbo.connectionOne);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_CONFIGVAL_CHANGED, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int configvalGetAsync(String key) {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_configval_get(
                xbo.connectionOne, key);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_CONFIGVAL_CHANGED, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int volumeGetAsync() {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_playback_volume_get(xbo.connectionOne);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYBACK_VOLUME_CHANGED, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    protected int playlistListAsync() {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_configval_list(xbo.connectionOne);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYLIST_CHANGED, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int mlibGetTitleAsync(long id) {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_get_info(
                xbo.connectionOne, id);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.USER_DEFINED_CALLBACK_2, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    protected int playlistGetCurrentIndexAsync() {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_playlist_current_pos(xbo.connectionOne);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYLIST_CURRENT_POSITION, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int getPlaybackStatusAsync() {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_playback_status(xbo.connectionOne);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYBACK_STATUS, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int mlibSelectAsync(String sql) {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_select(
                xbo.connectionOne, sql);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.USER_DEFINED_CALLBACK_1, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int mlibPlaylistListAsync(String name) {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_medialib_playlist_list(xbo.connectionOne, name);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYLIST_CHANGED, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    protected int mlibPlaylistsListAsync() {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_medialib_playlists_list(xbo.connectionOne);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_MEDIALIB_PLAYLIST_LOADED, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int mlibGetIDAsync(String url) {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_get_id(
                xbo.connectionOne, url);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYBACK_ID, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int currentIDAsync() {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_playback_current_id(xbo.connectionOne);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYBACK_ID, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int pluginsListAsync(int type) {
    	int t = getNextTID();
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_plugin_list(
                xbo.connectionOne, xmms_plugin_type_t.swigToEnum(type));
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.USER_DEFINED_CALLBACK_3, 
                Xmmsclient.convertIntToVoidP(t));
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }  
    

    /*
     * Following methods wait for the result and return to the caller. THEY
     * BLOCK!
     */
    public Dict configvalListSync() throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_configval_list(xbo.connectionTwo);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        xbo.lockDictForeach();
        Xmmsclient.xmmsc_result_dict_foreach(result,
                XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 
                Xmmsclient.convertIntToVoidP(0));
        Dict map = xbo.unlockDictForeach();
        Xmmsclient.xmmsc_result_unref(result);
        return map;
    }

    public String configvalGetSync(String key) throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_configval_get(
                xbo.connectionTwo, key);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        String value[] = new String[1];
        value[0] = "";
        Xmmsclient.xmmsc_result_get_dict_entry_str(result, "key", value);
        Xmmsclient.xmmsc_result_unref(result);
        return value[0];
    }

    public Dict volumeGetSync() throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_playback_volume_get(xbo.connectionTwo);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        xbo.lockDictForeach();
        Xmmsclient.xmmsc_result_dict_foreach(result,
                XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 
                Xmmsclient.convertIntToVoidP(0));
        Dict vol = xbo.unlockDictForeach();
        Xmmsclient.xmmsc_result_unref(result);
        return vol;
    }

    public Title mlibGetTitleSync(long id) throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_get_info(
                xbo.connectionTwo, id);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        xbo.lockTitle();
        if (sourcePref != null)
            Xmmsclient.xmmsc_result_source_preference_set(result, sourcePref);
        Xmmsclient.xmmsc_result_propdict_foreach(result,
                XmmsclientConstants.CALLBACK_PROPDICT_FOREACH_FUNCTION, 
                Xmmsclient.convertIntToVoidP(0));
        Title t = xbo.unlockTitle();
        Xmmsclient.xmmsc_result_unref(result);
        return t;
    }

    public int getPlaybackStatusSync() throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_playback_status(xbo.connectionTwo);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        long state[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, state);
        Xmmsclient.xmmsc_result_unref(result);
        return (int)state[0];
    }

    public List mlibSelectSync(String sql) throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_select(
                xbo.connectionTwo, sql);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        List entries = new LinkedList();
        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
            Xmmsclient.xmmsc_result_list_first(result);
            while (Xmmsclient.xmmsc_result_list_valid(result) == 1) {
                xbo.lockDictForeach();
                Xmmsclient.xmmsc_result_dict_foreach(result,
                        XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 
                        Xmmsclient.convertIntToVoidP(0));
                entries.add(xbo.unlockDictForeach());
                Xmmsclient.xmmsc_result_list_next(result);
            }
        }
        Xmmsclient.xmmsc_result_unref(result);
        return entries;
    }

    public List mlibPlaylistListSync(String name) throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_medialib_playlist_list(xbo.connectionTwo, name);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        ArrayList l = new ArrayList();
        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
            Xmmsclient.xmmsc_result_list_first(result);
            long id[] = new long[1];
            while (Xmmsclient.xmmsc_result_list_valid(result) == 1) {
                Xmmsclient.xmmsc_result_get_uint(result, id);
                l.add(new Long(id[0]));
                Xmmsclient.xmmsc_result_list_next(result);
            }
        }
        Xmmsclient.xmmsc_result_unref(result);
        return l;
    }

    public long mlibGetIDSync(String url) throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_get_id(
                xbo.connectionTwo, url);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        long id[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, id);
        Xmmsclient.xmmsc_result_unref(result);
        return id[0];
    }

    public long currentIDSync() throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_playback_current_id(xbo.connectionTwo);
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        long id[] = new long[1];
        Xmmsclient.xmmsc_result_get_uint(result, id);
        Xmmsclient.xmmsc_result_unref(result);
        return id[0];
    }

    public Dict pluginsListSync(int type) throws Xmms2Exception {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_plugin_list(
                xbo.connectionTwo, xmms_plugin_type_t.swigToEnum(type));
        Xmmsclient.xmmsc_result_wait(result);
        if (!xbo.isError(result).equals("")) {
            throw new Xmms2Exception(xbo.isError(result));
        }
        Dict map = new Dict();
        if (Xmmsclient.xmmsc_result_is_list(result) == 1) {
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
        }
        Xmmsclient.xmmsc_result_unref(result);
        return map;
    }
    
    public Playlist getPlaylist(){
    	return pl;
    }
    public String bindataAdd(short data[]) {
    	SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_bindata_add(
        		xbo.connectionTwo, data, data.length);
    	Xmmsclient.xmmsc_result_wait(result);
    	String hash[] = new String[1];
    	Xmmsclient.xmmsc_result_get_string(result, hash);
    	Xmmsclient.xmmsc_result_unref(result);
    	return hash[0];
    }
    public short[] bindataRetrieve(String hash) {
    	SWIGTYPE_p_xmmsc_result_St result = 
    		Xmmsclient.xmmsc_bindata_retrieve(xbo.connectionTwo, hash);
    	Xmmsclient.xmmsc_result_wait(result);
    	short[][] bindata = null;
    	Xmmsclient.xmmsc_result_get_bin_wrap(result, bindata);
    	Xmmsclient.xmmsc_result_unref(result);
    	return bindata[0];
    }
    public void bindataRemove(String hash) {
    	SWIGTYPE_p_xmmsc_result_St result = 
    		Xmmsclient.xmmsc_bindata_remove(xbo.connectionTwo, hash);
    	Xmmsclient.xmmsc_result_wait(result);
    	Xmmsclient.xmmsc_result_unref(result);
    }
}
