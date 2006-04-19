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
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;

import org.xmms2.xmms2bindings.SWIGTYPE_p_xmmsc_result_St;
import org.xmms2.xmms2bindings.Xmmsclient;
import org.xmms2.xmms2bindings.XmmsclientConstants;

/**
 * Use this class to work with Xmms2 in Java. call getInstance(), connect() and then do whatever 
 * you want until you call spinDown(). But take care on errors and set a Xmms2Listener to listen 
 * for events.
 * 
 * ExampleAsync:
 * 		Xmms2 bleh = Xmms2.getInstance("YourClientName");
 * 		bleh.connect();
 * 		bleh.addXmms2Listener(new Xmms2Listener(){
 * 				.....
 * 		});
 * 		bleh.play();
 * 		# Wait some time or do some other weird things ;)
 * 		bleh.spinDown();
 * 
 * ExampleSync:
 * 		Xmms2 bleh = Xmms2.getInstance("YourClientName");
 * 		bleh.connect();
 * 		Map configs = bleh.configvalListSync();
 * 		# Do some nifty things with the configs you got #
 * 		bleh.spinDown();
 * 
 * Don't use org.xmms2.xmms2bindings.*, org.xmms2.SpecialJNI and org.xmms2.JMain directly if you 
 * are using org.xmms2.Xmms2
 */

public final class Xmms2 {
	private static Xmms2 instance = null;
	protected String serverip = "", serverport = "";
	private ArrayList listeners;
	private Xmms2Backoffice xbo;
	protected String sourcePref[], source;
	
	private Xmms2(String clientname) throws UnsatisfiedLinkError, Xmms2Exception{
		try {
			System.loadLibrary("xmms2java");
		} catch (UnsatisfiedLinkError e){
			throw e;
		}
		
		xbo = new Xmms2Backoffice(this, clientname);
		listeners = new ArrayList();
	}
	public void addXmms2Listener(Xmms2Listener l){
		listeners.add(l);
	}
	public void removeXmms2Listener(Xmms2Listener l){
		if (listeners.contains(l))
			listeners.remove(l);
	}
	/**
	 * Sourcepreference which is used to work with propdicts. Set null here to use the default
	 * @param pref
	 */
	public void setSourcePreference(String pref[]){
		sourcePref = pref;
	}
	/**
	 * Source which is transmitted when editing entries. Set it to null or "" to use the default
	 * @param source
	 */
	public void setSource(String source){
		this.source = source;
	}
	/**
	 * Call that function on startup _before_ issuing any commands
	 * @throws Xmms2Exception 
	 *
	 */
	public void connect() throws Xmms2Exception{
		xbo.connect();
	}
	/**
	 * Enable the broadcasts
	 */
	public void enableBroadcasts(){
		xbo.enableBroadcasts();
	}

	protected void notifiyListeners(Method method, Xmms2Event e){
		for ( ListIterator it = listeners.listIterator(); it.hasNext();){
			try {
				method.invoke(it.next(), new Object[]{e});
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
	 * @return			Xmms2 instance
	 * @throws UnsatisfiedLinkError	if xmms2java cannot be found in java.library.path
	 * @throws Xmms2Exception 
	 */
	public static Xmms2 getInstance(String clientname) throws UnsatisfiedLinkError, Xmms2Exception{
		if (instance == null)
			instance = new Xmms2(clientname);
		return instance;
	}
	
	public boolean isConnected(){
		return xbo.connected;
	}
	
	/**
	 * Call that when you close your app to close the mainloop too
	 */
	public void spinDown(){
		xbo.spinDown();
	}
	
	/**
	 * Set serverip and serverport here. If both are set we accept them as is. 
	 * @param serverip
	 * @param serverport
	 */
	public void setConnectionParams(String serverip, String serverport){
		if (serverip != null && serverport != null){
			this.serverip = serverip;
			this.serverport = serverport;
		}
	}
	
	/*
	 * Following void returning functions work almost as their c-pendants
	 */
	private void handleNotifierTypePlayback(SWIGTYPE_p_xmmsc_result_St result){
		Xmmsclient.xmmsc_result_notifier_set(result, XmmsclientConstants.USER_DEFINED_CALLBACK_1, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void play(){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_start(xbo.connectionOne));
	}
	public void stop(){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_stop(xbo.connectionOne));
	}
	public void pause(){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_pause(xbo.connectionOne));
	}
	public void next(){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_set_next_rel(xbo.connectionOne, 1));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void prev(){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_set_next_rel(xbo.connectionOne, -1));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void jumpBy(int x){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_set_next_rel(xbo.connectionOne, x));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void shuffle(){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_shuffle(xbo.connectionOne));
	}
	public void clear(){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_clear(xbo.connectionOne));
	}
	public void addId(int id){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_add_id(xbo.connectionOne, id));
	}
	public void addUrl(String url){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_add(xbo.connectionOne, url));
	}
	public void insertId(int id, int position){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_insert_id(xbo.connectionOne, position, id));
	}
	public void insertUrl(String url, int position){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_insert(xbo.connectionOne, position, url));
	}
	public void removeIndex(int index){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_remove(xbo.connectionOne, index));
	}
	public void sort(String by){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_sort(xbo.connectionOne, by));
	}
	public void setNext(int index){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_set_next(xbo.connectionOne, index));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void move(int sourceIndex, int destIndex){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playlist_move(xbo.connectionOne, sourceIndex, destIndex));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void seek(int ms){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_seek_ms(xbo.connectionOne, ms));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void seekRel(int ms){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_seek_ms_rel(xbo.connectionOne, ms));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void seekSamples(int samples){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_seek_samples(xbo.connectionOne, samples));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void seekSamplesRel(int samples){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_seek_samples_rel(xbo.connectionOne, samples));
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_tickle(xbo.connectionOne));
	}
	public void configvalSet(String key, String val){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_configval_set(xbo.connectionOne, key, val));
	}
	public void configvalAdd(String key, String val){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_configval_register(xbo.connectionOne, key, val));
	}
	public void volumeSet(String channel, int value){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_playback_volume_set(xbo.connectionOne, channel, value));
	}
	public void killXmms2d(){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_quit(xbo.connectionOne));
	}
	public void mlibAddUrl(String url){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_add_entry(xbo.connectionOne, url));
	}
	public void mlibAddToPlaylist(String sql){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_add_to_playlist(xbo.connectionOne, sql));
	}
	public void mlibRemoveId(int id){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_remove_entry(xbo.connectionOne, id));
	}
	public void mlibProperySet(int id, String key, String value){
		if (source == null || source.equals(""))
			handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_entry_property_set(xbo.connectionOne, id, key, value));
		else handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_entry_property_set_with_source(xbo.connectionOne, id, 
				source, key, value));
	}
	public void mlibProperyRemove(int id, String key){
		if (source == null || source.equals(""))
			handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_entry_property_remove(xbo.connectionOne, id, key));
		else handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_entry_property_remove_with_source(xbo.connectionOne, id, 
				source, key));
	}
	public void saveCurrentPlaylist(String name){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_save_current(xbo.connectionOne, name));
	}
	public void mlibImportPlaylist(String url, String name){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_import(xbo.connectionOne, name, url));
	}
	public void mlibExportPlaylist(String name, String mime){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_export(xbo.connectionOne, name, mime));
	}
	public void mlibLoadPlaylist(String name){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_load(xbo.connectionOne, name));
	}
	public void mlibRemovePlaylist(String name){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_playlist_remove(xbo.connectionOne, name));
	}
	public void mlibRehash(int id){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_rehash(xbo.connectionOne, id));
	}
	public void mlibPathImport(String url){
		handleNotifierTypePlayback(Xmmsclient.xmmsc_medialib_path_import(xbo.connectionOne, url));
	}
	
	
	/*
	 * Following methods notify all Xmms2Listeners with Xmms2Events. 
	 */
	public void configvalListAsync(){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_configval_list(xbo.connectionOne);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_CONFIGVAL_CHANGED, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void configvalGetAsync(String key){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_configval_get(xbo.connectionOne, key);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_CONFIGVAL_CHANGED, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void volumeGetAsync(){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_playback_volume_get(xbo.connectionOne);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_PLAYBACK_VOLUME_CHANGED, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void playlistListAsync(){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_configval_list(xbo.connectionOne);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_PLAYLIST_CHANGED, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void mlibGetTitleAsync(long id){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_get_info(xbo.connectionOne, id);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.USER_DEFINED_CALLBACK_2, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void playlistGetCurrentIndexAsync(){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_playlist_current_pos(xbo.connectionOne);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_PLAYLIST_CURRENT_POSITION, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void getPlaybackStatusAsync(){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_playback_status(xbo.connectionOne);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_PLAYBACK_STATUS, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void mlibSelectAsync(String sql){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_select(xbo.connectionOne, sql);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.USER_DEFINED_CALLBACK_1, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void mlibPlaylistListAsync(String name){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_playlist_list(xbo.connectionOne, name);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_PLAYLIST_CHANGED, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void mlibPlaylistsListAsync(){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_playlists_list(xbo.connectionOne);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_MEDIALIB_PLAYLIST_LOADED, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void mlibGetIDAsync(String url){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_get_id(xbo.connectionOne, url);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_PLAYBACK_ID, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void currentIDAsync(){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_playback_current_id(xbo.connectionOne);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.CALLBACK_PLAYBACK_ID, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	public void pluginsListAsync(int type){
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_plugin_list(xbo.connectionOne, type);
		Xmmsclient.xmmsc_result_notifier_set(result, 
				XmmsclientConstants.USER_DEFINED_CALLBACK_3, 0);
		Xmmsclient.xmmsc_result_unref(result);
	}
	
	
	
	/*
	 * Following methods wait for the result and return to the caller. THEY BLOCK! 
	 */
	public Map configvalListSync() throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_configval_list(xbo.connectionTwo);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		xbo.lockDictForeach();
		Xmmsclient.xmmsc_result_dict_foreach(result, XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 0);
		Map map = xbo.unlockDictForeach();
		Xmmsclient.xmmsc_result_unref(result);
		return map;
	}
	public String configvalGetSync(String key) throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_configval_get(xbo.connectionTwo, key);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		String value[] = new String[1];
		value[0] = "";
		Xmmsclient.xmmsc_result_get_dict_entry_str(result, "key", value);
		Xmmsclient.xmmsc_result_unref(result);
		return value[0];
	}
	public Map volumeGetSync() throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_playback_volume_get(xbo.connectionTwo);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		xbo.lockDictForeach();
		Xmmsclient.xmmsc_result_dict_foreach(result, XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 0);
		Map vol = xbo.unlockDictForeach();
		Xmmsclient.xmmsc_result_unref(result);
		return vol;
	}
	public List playlistListSync() throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_configval_list(xbo.connectionTwo);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		ArrayList pl = new ArrayList();
		if (Xmmsclient.xmmsc_result_is_list(result) == 1){
			Xmmsclient.xmmsc_result_list_first(result);
			long id[] = new long[1];
			while (Xmmsclient.xmmsc_result_list_valid(result) == 1){
				Xmmsclient.xmmsc_result_get_uint(result, id);
				pl.add(new Long(id[0]));
				Xmmsclient.xmmsc_result_list_next(result);
			}
		}
		Xmmsclient.xmmsc_result_unref(result);
		return pl;
	}
	public Title mlibGetTitleSync(long id) throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_get_info(xbo.connectionTwo, id);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		xbo.lockTitle();
		if (sourcePref != null)
			Xmmsclient.xmmsc_result_source_preference_set(result, sourcePref);
		Xmmsclient.xmmsc_result_propdict_foreach(result, 
				XmmsclientConstants.CALLBACK_PROPDICT_FOREACH_FUNCTION, 0);
		Title t = xbo.unlockTitle();
		Xmmsclient.xmmsc_result_unref(result);
		return t;
	}
	public long playlistGetCurrentIndexSync() throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_playlist_current_pos(xbo.connectionTwo);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		long[] pos = new long[1];
		Xmmsclient.xmmsc_result_get_uint(result, pos);
		Xmmsclient.xmmsc_result_unref(result);
		return pos[0];
	}
	public long getPlaybackStatusSync() throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_playback_status(xbo.connectionTwo);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		long state[] = new long[1];
		Xmmsclient.xmmsc_result_get_uint(result, state);
		Xmmsclient.xmmsc_result_unref(result);
		return state[0];
	}
	public List mlibSelectSync(String sql) throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_select(xbo.connectionTwo, sql);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		List entries = new LinkedList();
		if (Xmmsclient.xmmsc_result_is_list(result) == 1){
			Xmmsclient.xmmsc_result_list_first(result);
			while (Xmmsclient.xmmsc_result_list_valid(result) == 1){
				xbo.lockDictForeach();
				Xmmsclient.xmmsc_result_dict_foreach(result, 
						XmmsclientConstants.CALLBACK_DICT_FOREACH_FUNCTION, 0);
				entries.add(xbo.unlockDictForeach());
				Xmmsclient.xmmsc_result_list_next(result);
			}
		}
		Xmmsclient.xmmsc_result_unref(result);
		return entries;
	}
	public List mlibPlaylistListSync(String name) throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_playlist_list(xbo.connectionTwo, name);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		ArrayList pl = new ArrayList();
		if (Xmmsclient.xmmsc_result_is_list(result) == 1){
			Xmmsclient.xmmsc_result_list_first(result);
			long id[] = new long[1];
			while (Xmmsclient.xmmsc_result_list_valid(result) == 1){
				Xmmsclient.xmmsc_result_get_uint(result, id);
				pl.add(new Long(id[0]));
				Xmmsclient.xmmsc_result_list_next(result);
			}
		}
		Xmmsclient.xmmsc_result_unref(result);
		return pl;
	}
	public List mlibPlaylistsListSync() throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_playlists_list(xbo.connectionTwo);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		List entries = new LinkedList();
		if (Xmmsclient.xmmsc_result_is_list(result) == 1){
			Xmmsclient.xmmsc_result_list_first(result);
			while (Xmmsclient.xmmsc_result_list_valid(result) == 1){
				String name[] = new String[1];
				Xmmsclient.xmmsc_result_get_string(result, name);
				entries.add(name[0]);
				Xmmsclient.xmmsc_result_list_next(result);
			}
		}
		Xmmsclient.xmmsc_result_unref(result);
		return entries;
	}
	public long mlibGetIDSync(String url) throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_medialib_get_id(xbo.connectionTwo, url);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		long id[] = new long[1];
		Xmmsclient.xmmsc_result_get_uint(result, id);
		Xmmsclient.xmmsc_result_unref(result);
		return id[0];
	}
	public long currentIDSync() throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_playback_current_id(xbo.connectionTwo);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		long id[] = new long[1];
		Xmmsclient.xmmsc_result_get_uint(result, id);
		Xmmsclient.xmmsc_result_unref(result);
		return id[0];
	}
	public Map pluginsListSync(int type) throws Xmms2Exception{
		SWIGTYPE_p_xmmsc_result_St result = Xmmsclient.xmmsc_plugin_list(xbo.connectionTwo, type);
		Xmmsclient.xmmsc_result_wait(result);
		if (!xbo.isError(result).equals("")){
			throw new Xmms2Exception(xbo.isError(result));
		}
		HashMap map = new HashMap();
		if (Xmmsclient.xmmsc_result_is_list(result) == 1){
			Xmmsclient.xmmsc_result_list_first(result);
			String shortDes[] = new String[1];
			String longDes[] = new String[1];
			while (Xmmsclient.xmmsc_result_list_valid(result) == 1){
				if (Xmmsclient.xmmsc_result_get_dict_entry_str (result,
						"shortname", shortDes) == 1){
					Xmmsclient.xmmsc_result_get_dict_entry_str (result,
							"shortname", longDes);
					map.put(shortDes[0], longDes[0]);
				}
				Xmmsclient.xmmsc_result_list_next(result);
			}
		}
		Xmmsclient.xmmsc_result_unref(result);
		return map;
	}
}
