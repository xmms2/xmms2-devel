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
#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

#ifdef __cplusplus
{
#endif

#include <xmmsclient/xmmsclient.h>
#include <jni.h>

/*
 * we need these 3 objects, no way otherwise
 */
JavaVM *jvm;
jobject globalObj;
jobject globalMainloopObj;

/*
 * all mids used by globalMainloopObj and globalObj (callbackfunctions)
 */
jmethodID disconnect_mid;
jmethodID lock_mid;
jmethodID unlock_mid;
jmethodID io_want_out_mid;
jmethodID configval_changed_mid;
jmethodID playlist_changed_mid;
jmethodID playlist_current_pos_mid;
jmethodID playback_id_mid;
jmethodID playback_volume_changed_mid;
jmethodID playback_status_mid;
jmethodID medialib_entry_changed_mid;
jmethodID medialib_entry_added_mid;
jmethodID medialib_playlist_loaded_mid;
jmethodID mediainfo_reader_status_mid;
jmethodID playback_playtime_mid;
jmethodID visualization_data_mid;
jmethodID signal_mediareader_mid;
jmethodID dict_foreach_mid;
jmethodID propdict_foreach_mid;
jmethodID user_defined_1_mid;
jmethodID user_defined_2_mid;
jmethodID user_defined_3_mid;

/**
 * all callbackfunctions that can be used somehow within java
 */
extern void disconnect_callback (void *error);
extern void lock_function (void *v);
extern void unlock_function (void *v);
extern void io_want_out_callback (int val, void *error);
extern void callback_configval_changed (xmmsc_result_t *res, void *user_data);
extern void callback_playlist_changed (xmmsc_result_t *res, void *user_data);
extern void callback_playlist_current_position (xmmsc_result_t *res, void *user_data);
extern void callback_playback_id (xmmsc_result_t *res, void *user_data);
extern void callback_playback_status (xmmsc_result_t *res, void *user_data);
extern void callback_playback_volume_changed (xmmsc_result_t *res, void *user_data);
extern void callback_medialib_entry_changed (xmmsc_result_t *res, void *user_data);
extern void callback_medialib_entry_added (xmmsc_result_t *res, void *user_data);
extern void callback_medialib_playlist_loaded (xmmsc_result_t *res, void *user_data);
extern void callback_mediainfo_reader_status (xmmsc_result_t *res, void *user_data);
extern void signal_playback_playtime (xmmsc_result_t *res, void *user_data);
extern void signal_visualization_data (xmmsc_result_t *res, void *user_data);
extern void signal_mediareader_unindexed (xmmsc_result_t *res, void *user_data);
extern void user_defined_callback_1 (xmmsc_result_t *res, void *user_data);
extern void user_defined_callback_2 (xmmsc_result_t *res, void *user_data);
extern void user_defined_callback_3 (xmmsc_result_t *res, void *user_data);
extern void callback_dict_foreach_function (const void *key, 
                                           xmmsc_result_value_type_t type,
                                           const void *value,
                                           void *user_data);
extern void callback_propdict_foreach_function (const void *key, 
                                               xmmsc_result_value_type_t type,
                                               const void *value,
                                               const char *source, 
                                               void *user_data);
#ifdef __cplusplus
}
#endif

#endif
