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

import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.IOException;

import org.xmms2.xmms2bindings.Xmmsclient;
import org.xmms2.xmms2bindings.XmmsclientConstants;
import org.xmms2.xmms2bindings.SWIGTYPE_p_xmmsc_connection_St;
import org.xmms2.xmms2bindings.SWIGTYPE_p_xmmsc_result_St;

/**
 * Java way of using a mainloop. Just create a new Object and give a
 * Callbacks-object, the mainloop does the rest :). Don't use this class if you
 * are using org.xmms2.Xmms2 and don't use org.xmms2.SpecialJNI if you are using
 * org.xmms2.JMain
 */

public final class JMain extends Thread {
    private SWIGTYPE_p_xmmsc_connection_St myConnection;
    private boolean running = false;
    private Boolean stopped = Boolean.FALSE;

    /**
     * constructor to set up the main loop and all needed callbacks
     * 
     * @param cb
     *            CallbacksInterface-object to call when something arrives
     * @param conn
     *            connection to listen on
     */
    public JMain(CallbacksListener cb, SWIGTYPE_p_xmmsc_connection_St conn) {
        myConnection = conn;
        SpecialJNI.setENV(cb);
    }

    /**
     * The run method - our little mainloop. First call SpecialJNI.setENV(this),
     * which is a native method and sets "this" as GlobalRef for the
     * callback-object in c and sets the pointer jvm, which is needed to call
     * java methods outside the JNI functions. Furthermore, init the
     * callback-methods (set xmms2d's result_notifiers). While the
     * file-descriptor is valid, we will wait for new input and if something is
     * available, call Xmmsclient.xmmsc_io_in_handle(myConnection)
     * 
     */
    public void run() {
    	running = true;
        FileDescriptor fd = new FileDescriptor();
        SpecialJNI.getFD(fd, Xmmsclient.getPointerToConnection(myConnection));
        SpecialJNI.setupMainloop(this, Xmmsclient
                .getPointerToConnection(myConnection));
        FileInputStream in = new FileInputStream(fd);
        try {
            while (fd.valid() && running) {
                Thread.sleep(20);
                if (in.available() > 0)
                    Xmmsclient.xmmsc_io_in_handle(myConnection);
                if (Xmmsclient.xmmsc_io_want_out(myConnection) == 1)
                	Xmmsclient.xmmsc_io_out_handle(myConnection);
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        } catch (IOException io) {
            System.err
                    .println("my fd went to nowhere - should be ok if you just shut down");
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            try {
                if (in != null)
                    in.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        synchronized (stopped){
        	stopped.notifyAll();
        	stopped = Boolean.TRUE;
        }
    }

    /**
     * detach from the mainloop
     * 
     */
    public void spinDown() {
    	Xmmsclient.xmmsc_io_disconnect(myConnection);
        running = false;
        synchronized (stopped){
        	while (stopped == Boolean.FALSE)
				try {
					stopped.wait();
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
        }
    }

    public void setConfigvalChangedCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_configval_changed(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_CONFIGVAL_CHANGED, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setPlaylistCurrentPositionCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_playlist_current_pos(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYLIST_CURRENT_POSITION,
                user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setPlaybackCurrentIDCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_playback_current_id(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYBACK_ID, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setPlaybackVolumeChangedCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_playback_volume_changed(myConnection);
        Xmmsclient
                .xmmsc_result_notifier_set(result,
                        XmmsclientConstants.CALLBACK_PLAYBACK_VOLUME_CHANGED,
                        user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setPlaybackStatusCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_playback_status(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYBACK_STATUS, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setPlaylistChangedCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_playlist_changed(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_PLAYLIST_CHANGED, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setMedialibEntryChangedCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_medialib_entry_changed(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_MEDIALIB_ENTRY_CHANGED, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setMedialibEntryAddedCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_medialib_entry_added(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_MEDIALIB_ENTRY_ADDED, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setMedialibPlaylistLoadedCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_medialib_playlist_loaded(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.CALLBACK_MEDIALIB_PLAYLIST_LOADED,
                user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setMediainfoReaderStatusCallback(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_broadcast_mediainfo_reader_status(myConnection);
        Xmmsclient
                .xmmsc_result_notifier_set(result,
                        XmmsclientConstants.CALLBACK_MEDIAINFO_READER_STATUS,
                        user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setPlaybackPlaytimeSignal(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_signal_playback_playtime(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.SIGNAL_PLAYBACK_PLAYTIME, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setVisualizationDataSignal(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_signal_visualisation_data(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.SIGNAL_VISUALIZATION_DATA, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void setMediareaderUnindexedSignal(int user_data) {
        SWIGTYPE_p_xmmsc_result_St result = Xmmsclient
                .xmmsc_signal_mediainfo_reader_unindexed(myConnection);
        Xmmsclient.xmmsc_result_notifier_set(result,
                XmmsclientConstants.SIGNAL_MEDIAREADER_UNINDEXED, user_data);
        Xmmsclient.xmmsc_result_unref(result);
    }

    public void callbackIOWantOut(int val, int user_data) {
        if (val == 1)
            Xmmsclient.xmmsc_io_out_handle(myConnection);
    }
}
