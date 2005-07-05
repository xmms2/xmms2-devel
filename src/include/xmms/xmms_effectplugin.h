/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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




#ifndef _XMMS_EFFECTPLUGIN_H_
#define _XMMS_EFFECTPLUGIN_H_

#include "xmms/xmms_effect.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_medialib.h"

typedef void (*xmms_effect_new_method_t) (xmms_effect_t *);
typedef void (*xmms_effect_destroy_method_t) (xmms_effect_t *);
typedef gboolean (*xmms_effect_format_set_method_t) (xmms_effect_t *, xmms_audio_format_t *);
typedef void (*xmms_effect_current_mlib_entry_method_t) (xmms_effect_t *, xmms_medialib_entry_t);
typedef void (*xmms_effect_process_method_t) (xmms_effect_t *, xmms_sample_t *, guint);

#define XMMS_PLUGIN_METHOD_NEW_TYPE xmms_effect_new_method_t
#define XMMS_PLUGIN_METHOD_DESTROY_TYPE xmms_effect_destroy_method_t
#define XMMS_PLUGIN_METHOD_FORMAT_SET_TYPE xmms_effect_format_set_method_t
#define XMMS_PLUGIN_METHOD_CURRENT_MEDIALIB_ENTRY_TYPE xmms_effect_current_mlib_entry_method_t
#define XMMS_PLUGIN_METHOD_PROCESS_TYPE xmms_effect_process_method_t


#endif
