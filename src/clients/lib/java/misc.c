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

#include <stdio.h>
#include <xmmsclient/xmmsclient.h>
#include <jni.h>
#include <callbacks.h>

jmethodID get_method_id (const char* methodname, const char* sig, JNIEnv *environment, jclass cls);

/*
 * called from the mainloop to get a valid framepointer object to be used in java
 */
JNIEXPORT void JNICALL 
Java_org_xmms2_wrapper_SpecialJNI_getFD (JNIEnv *env, jclass _ignore, jobject fdobj, jlong jarg1) 
{
	jfieldID field_fd;
	jclass class_fdesc;
	int fd = 0;
	xmmsc_connection_t *conn_ptr = (xmmsc_connection_t *) 0;
	
	conn_ptr = *(xmmsc_connection_t **)(void *)&jarg1;

	fd = xmmsc_io_fd_get (conn_ptr);

	class_fdesc = (*env)->GetObjectClass (env, fdobj);
	field_fd = (*env)->GetFieldID (env, class_fdesc, "fd", "I");
	(*env)->SetIntField (env, fdobj, field_fd, fd);
}

/*
 * sets the environment (callbackfunctions, jvm pointer, global object, ...
 */
JNIEXPORT void JNICALL 
Java_org_xmms2_wrapper_SpecialJNI_setENV (JNIEnv *jenv, jclass cls, jobject myobject)
{
	jclass clazz;
	globalObj = (*jenv)->NewGlobalRef (jenv, myobject);
	if (jvm == NULL)
	{
		(*jenv)->GetJavaVM (jenv,&jvm);
	}

	clazz = 
	        (*jenv)->GetObjectClass (jenv, myobject);
	disconnect_mid = 
	        get_method_id ("callbackDisconnect", "(I)V", jenv, clazz);
	lock_mid = 
	        get_method_id ("lockFunction", "(I)V", jenv, clazz);
	unlock_mid = 
	        get_method_id ("unlockFunction", "(I)V", jenv, clazz);
	configval_changed_mid = 
	        get_method_id ("callbackConfigvalChanged", "(JI)V", jenv, clazz);
	playlist_changed_mid = 
	        get_method_id("callbackPlaylistChanged", "(JI)V", jenv, clazz);
	playback_volume_changed_mid = 
	        get_method_id ("callbackPlaybackVolumeChanged", "(JI)V", jenv, clazz);
	playlist_current_pos_mid = 
	        get_method_id ("callbackPlaylistCurrentPosition", "(JI)V", jenv, clazz);
	playback_id_mid = 
	        get_method_id ("callbackPlaybackID", "(JI)V", jenv, clazz);
	playback_status_mid = 
	        get_method_id ("callbackPlaybackStatus", "(JI)V", jenv, clazz);
	medialib_entry_changed_mid = 
	        get_method_id ("callbackMedialibEntryChanged", "(JI)V", jenv, clazz);
	medialib_entry_added_mid = 
	        get_method_id ("callbackMedialibEntryAdded", "(JI)V", jenv, clazz);
	medialib_playlist_loaded_mid = 
	        get_method_id ("callbackMedialibPlaylistLoaded", "(JI)V", jenv, clazz);
	mediainfo_reader_status_mid = 
	        get_method_id ("callbackMediainfoReaderStatus", "(JI)V", jenv, clazz);
	playback_playtime_mid = 
	        get_method_id ("signalPlaybackPlaytime", "(JI)V", jenv, clazz);
	visualization_data_mid = 
	        get_method_id ("signalVisualizationData", "(JI)V", jenv, clazz);
	signal_mediareader_mid = 
	        get_method_id ("signalMediareaderUnindexed", "(JI)V", jenv, clazz);
	dict_foreach_mid = 
	        get_method_id ("callbackDictForeachFunction", 
		              "(Ljava/lang/String;ILjava/lang/String;I)V",
	                      jenv,
	                      clazz);
	propdict_foreach_mid = 
	        get_method_id ("callbackPropdictForeachFunction", 
		              "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;I)V",
	                      jenv,
	                      clazz);
	user_defined_1_mid = 
	        get_method_id ("userDefinedCallback1", "(JI)V", jenv, clazz);
	user_defined_2_mid = 
	        get_method_id ("userDefinedCallback2", "(JI)V", jenv, clazz);
	user_defined_3_mid = 
	        get_method_id ("userDefinedCallback3", "(JI)V", jenv, clazz);
}

/*
 * return methodid for given function and parameters
 */
jmethodID 
get_method_id (const char* methodname, const char* sig, JNIEnv *environment, jclass cls)
{
	return (*environment)->GetMethodID (environment, cls, methodname, sig);
}

/*
 * called by the mainloop to set mainloop specific things up
 */
JNIEXPORT void JNICALL 
Java_org_xmms2_wrapper_SpecialJNI_setupMainloop (JNIEnv *jenv, jclass cls, jobject myobject, jlong jarg1)
{
	xmmsc_connection_t *conn_ptr = (xmmsc_connection_t *) 0;
	jclass clazz;
	conn_ptr = *(xmmsc_connection_t **)(void *)&jarg1;
	globalMainloopObj = (*jenv)->NewGlobalRef (jenv, myobject);
	
	if (jvm == NULL) {
		(*jenv)->GetJavaVM(jenv,&jvm);
	}

	clazz = (*jenv)->GetObjectClass(jenv, myobject);
	io_want_out_mid = get_method_id ("callbackIOWantOut", "(II)V", jenv, clazz);
	
	//xmmsc_io_need_out_callback_set (conn_ptr, io_want_out_callback, 0);
}

