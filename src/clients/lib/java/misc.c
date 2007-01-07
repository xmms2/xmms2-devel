/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
#include <limits.h>
#include <xmmsclient/xmmsclient.h>
#include <xmms/xmms_defs.h>
#include <jni.h>
#include <callbacks.h>

jmethodID get_method_id (const char* methodname, const char* sig, JNIEnv *environment, jclass cls);

/*
 * called from the mainloop to get a valid framepointer object to be used in java
 */
JNIEXPORT void JNICALL 
Java_org_xmms2_wrapper_xmms2bindings_XmmsclientJNI_getFD (JNIEnv *env, jclass _ignore, jobject fdobj, jobject connection) 
{
	jfieldID field_fd, field_ptr;
	jclass class_fdesc;
	xmmsc_connection_t *conn_ptr;
	int fd = 0;
	jclass cls;

	cls = (*env)->GetObjectClass (env, connection);
	field_ptr = (*env)->GetFieldID (env, cls, "swigCPtr", "J");

	conn_ptr = (xmmsc_connection_t*)(*env)->GetLongField (env, connection, field_ptr);

	fd = xmmsc_io_fd_get (conn_ptr);

	class_fdesc = (*env)->GetObjectClass (env, fdobj);
	field_fd = (*env)->GetFieldID (env, class_fdesc, "fd", "I");
	(*env)->SetIntField (env, fdobj, field_fd, fd);
}

/*
 * sets the environment (callbackfunctions, jvm pointer, global object, ...
 */
JNIEXPORT void JNICALL 
Java_org_xmms2_wrapper_xmms2bindings_XmmsclientJNI_setENV (JNIEnv *jenv, jclass cls, jobject myobject)
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
Java_org_xmms2_wrapper_xmms2bindings_XmmsclientJNI_setupMainloop (JNIEnv *jenv,
                                                 jclass cls,
						 jobject myobject,
						 jobject connection)
{
	jclass clazz, cls2;
	jfieldID field_ptr;
	xmmsc_connection_t *conn_ptr;

	cls2 = (*jenv)->GetObjectClass ( jenv, connection);
	field_ptr = (*jenv)->GetFieldID (jenv, cls2, "swigCPtr", "J");

	conn_ptr = (xmmsc_connection_t*)(*jenv)->GetLongField (jenv, connection, field_ptr);

	globalMainloopObj = (*jenv)->NewGlobalRef (jenv, myobject);
	
	if (jvm == NULL) {
		(*jenv)->GetJavaVM(jenv,&jvm);
	}

	clazz = (*jenv)->GetObjectClass(jenv, myobject);
	io_want_out_mid = get_method_id ("callbackIOWantOut", "(II)V", jenv, clazz);
}

JNIEXPORT jbyteArray JNICALL
Java_org_xmms2_wrapper_xmms2bindings_XmmsclientJNI_xmmsc_1result_1get_1byte (JNIEnv *jenv,
                                                                             jclass caller,
									     jobject result)
{
	unsigned char *c_byte;
	unsigned int c_length;
	xmmsc_result_t *result_address;
	int ret_value = -1, i = 0;
	jclass cls;
	jfieldID field_ptr;

	cls = (*jenv)->GetObjectClass ( jenv, result);
	field_ptr = (*jenv)->GetFieldID (jenv, cls, "swigCPtr", "J");

	result_address = (xmmsc_result_t*)(*jenv)->GetLongField (jenv, result, field_ptr);

	ret_value = xmmsc_result_get_bin(result_address, &c_byte, &c_length);

	if (ret_value && c_length > 0) {
		jbyteArray row = (jbyteArray)(*jenv)->NewByteArray(jenv, c_length);
		
			(*jenv)->SetByteArrayRegion ( jenv,
			                              (jbyteArray)row,
		                                      (jsize)0,
		                                      c_length,
			                              (jbyte *)c_byte);
		return row;
	}
	return NULL;
}

JNIEXPORT jstring JNICALL
Java_org_xmms2_wrapper_xmms2bindings_XmmsclientJNI_xmmsc_1get_1userconfdir (JNIEnv *jenv,
                                                                            jclass caller)
{
	jstring jresult = 0;
	char result[PATH_MAX];

	xmmsc_userconfdir_get(result, PATH_MAX);
	if(result)
	{
		jresult = (*jenv)->NewStringUTF(jenv, result);
		return jresult;
	}
	return NULL;
}
