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

/*
 * This file and its functions is used to handle the callbacks from c to java.
 * It's somehow not nice to read but the only way i know.
 * There's no docu listed here since all public functions work the same as the ones
 * from xmmsclient.h. Just check docu there
 */

#include <stdio.h>
#include <xmmsclient/xmmsclient.h>
#include <callbacks.h>
#include <misc.h>
#include <jni.h>


void run_java_callback_result_void (xmmsc_result_t *res, jmethodID mid, void *user_data);
void run_java_callback_void (void *r, jmethodID mid);
void val2jval (JNIEnv *environment, const void *val, xmmsc_result_value_type_t type, jstring *jval);
JNIEnv *checkEnv ();


void 
disconnect_callback (void *error)
{
	run_java_callback_void (error, disconnect_mid);
}

void
lock_function (void *v)
{
	run_java_callback_void(v, lock_mid);
}

void 
unlock_function (void *v)
{
	run_java_callback_void(v, unlock_mid);
}

void 
io_want_out_callback (int val, void *error)
{
	jobject callbackObject;
	JNIEnv *environment = checkEnv ();  
	if (environment == NULL) {
		return;
	}
	
	callbackObject = (*environment)->
	                         NewLocalRef (environment, globalMainloopObj);
	
	if (io_want_out_mid == 0) {
		return;
	}

	(*environment)->CallObjectMethod (environment, 
	                                 callbackObject, 
	                                 io_want_out_mid, 
	                                 val, 
	                                 *((int*)error));
}

void 
callback_configval_changed (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              configval_changed_mid, 
	                              user_data);
}

void 
callback_playlist_changed (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              playlist_changed_mid, 
	                              user_data);
}

void 
callback_playlist_current_position (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              playlist_current_pos_mid, 
	                              user_data);
}

void 
callback_playback_id (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              playback_id_mid, 
	                              user_data);
}

void 
callback_playback_status (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              playback_status_mid, 
	                              user_data);
}

void 
callback_playback_volume_changed (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              playback_volume_changed_mid, 
	                              user_data);
}

void 
callback_medialib_entry_changed (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              medialib_entry_changed_mid, 
	                              user_data);
}


void 
callback_medialib_entry_added (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              medialib_entry_added_mid, 
	                              user_data);
}

void 
callback_mediainfo_reader_status (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              mediainfo_reader_status_mid, 
	                              user_data);
}

void 
callback_medialib_playlist_loaded (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              medialib_playlist_loaded_mid, 
	                              user_data);
}

void 
signal_playback_playtime (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              playback_playtime_mid, 
	                              user_data);
}

void 
signal_visualization_data (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              visualization_data_mid, 
	                              user_data);
}

void 
signal_mediareader_unindexed (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              signal_mediareader_mid, 
	                              user_data);
}

void 
user_defined_callback_1 (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              user_defined_1_mid, 
	                              user_data);
}

void 
user_defined_callback_2 (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              user_defined_2_mid, 
	                              user_data);
}

void 
user_defined_callback_3 (xmmsc_result_t *res, void *user_data)
{
	run_java_callback_result_void (res, 
	                              user_defined_3_mid, 
	                              user_data);
}

void 
callback_dict_foreach_function (const void *key, 
                               xmmsc_result_value_type_t type, 
                               const void *value, 
                               void *user_data)
{
	jint jres_val = 0;
	jstring jkey, jvalue;
	jobject callbackObject;
	JNIEnv *environment = checkEnv ();
	
	if (key == NULL || value == NULL) {
		return;
	}

	if (environment == NULL) {
		return;
	}
	
	callbackObject = (*environment)->
	                         NewLocalRef (environment, globalObj);
	if (dict_foreach_mid == 0) {
	        return;
	}
	
	val2jval (environment, value, type, &jvalue);

	jkey = (*environment)->NewStringUTF (environment, (char*)key);
	*(xmmsc_result_value_type_t *)(void *)&jres_val = type;
	(*environment)->CallObjectMethod (environment, 
	                                 callbackObject, 
	                                 dict_foreach_mid, 
	                                 jkey, 
	                                 jres_val, 
	                                 jvalue, 
	                                 *((int*)user_data));
}

void 
callback_propdict_foreach_function (const void *key, 
                                   xmmsc_result_value_type_t type, 
                                   const void *value, 
                                   const char *source, 
                                   void *user_data)
{
	jint jres_val = 0;
	jstring jkey, jvalue, jsource;
	jobject callbackObject;
	JNIEnv *environment = checkEnv ();
	
	if (environment == NULL) {
		return;
	}
	
	if (key == NULL || value == NULL) {
		return;
	}
	
	callbackObject = (*environment)->
	                         NewLocalRef (environment, globalObj);
	
	if (propdict_foreach_mid == 0) {
	        return;
	}
	
	val2jval (environment, value, type, &jvalue);

	jkey = (*environment)->NewStringUTF (environment, (char*)key);
	jsource = (*environment)->NewStringUTF (environment, (char*)source);
	
	*(xmmsc_result_value_type_t *)(void *)&jres_val = type;
	(*environment)->CallObjectMethod (environment, 
	                                 callbackObject, 
	                                 propdict_foreach_mid, 
	                                 jkey,
	                                 jres_val, 
	                                 jvalue, 
	                                 jsource, 
	                                 *((int*)user_data));
}

/*
 * called in (prop)dict_foreach to get the right value and put it into a string
 */
void 
val2jval (JNIEnv *environment, const void *val, xmmsc_result_value_type_t type, jstring *jval)
{
	char tval[300];

	if (type == XMMSC_RESULT_VALUE_TYPE_STRING) {
		snprintf (tval, 300, "%s", (char*)val);
	}
	else if (type == XMMSC_RESULT_VALUE_TYPE_UINT32) {
		snprintf (tval, 300, "%d", ((unsigned int)val));
	}
	else if (type == XMMSC_RESULT_VALUE_TYPE_INT32) {
		snprintf (tval, 300, "%d", ((int)val));
	}

	*jval = (*environment)->NewStringUTF (environment, tval);
}

/*
 * used for calling functions of the form (result*, void*), in case of java these
 * functions get of the form (long, int)
 */
void
run_java_callback_result_void (xmmsc_result_t *res, jmethodID mid, void *user_data)
{
	jlong jresult = 0;
	jobject callbackObject;
	JNIEnv *environment = checkEnv ();
    
	if (environment == NULL) {
		return;
	}
		
	callbackObject = (*environment)->
	        NewLocalRef(environment, globalObj);
	
	if (mid == 0) {
	        return;
	}
   
	*(xmmsc_result_t **)(void *)&jresult = res;
	(*environment)->CallObjectMethod (environment, 
	                                 callbackObject, 
	                                 mid, 
	                                 jresult, 
	                                 *((int*)user_data));
}
/*
 * call callbacks of form (void*), in java wrapped to (int)
 */
void 
run_java_callback_void (void *v, jmethodID mid)
{
	jobject callbackObject;
	JNIEnv *environment = checkEnv ();
	
	if (environment == NULL) {
		return;
	}
	
	callbackObject = (*environment)->
	        NewLocalRef (environment, globalObj);   
	if (mid == 0) {
        	return;
	}

	(*environment)->CallObjectMethod (environment,
	                                 callbackObject, 
	                                 mid, 
	                                 *((int*)v));
}
/*
 * this function is here so that swig can wrap it. The dev can call it later
 * in his callbackfunctions to get results from the long values
 */
xmmsc_result_t* 
getResultFromPointer (jlong val)
{
	return *(xmmsc_result_t **)(void *)&val;
}

/*
 * swig rewrites this function so we can use it in java. it returns a pointer
 * to a connection object.
 */
jlong 
getPointerToConnection (xmmsc_connection_t *c)
{
	jlong connection;
	*(xmmsc_connection_t **)(void *)&connection = c;
	return connection;
}

/*
 * used internally to check for an existing environment. 
 * returns NULL if something failed
 */
JNIEnv* 
checkEnv ()
{
	JNIEnv *env = NULL;
	if (jvm == NULL || globalObj == NULL) {
		fprintf (stderr, 
		        "No JVM-pointer given or no callbackobject given! Call setEnv() first");
		return NULL;
	}
	
        (*jvm)->AttachCurrentThread (jvm, (void *)&env, NULL);
	return env;
}

