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




#ifndef _XMMS_OUTPUT_INT_H_
#define _XMMS_OUTPUT_INT_H_

#include "xmms/plugin.h"
#include "xmms/config.h"
#include "xmms/decoder.h"

/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_output_t *xmms_output_new (xmms_plugin_t *plugin, xmms_playlist_t *playlist);

xmms_medialib_entry_t xmms_output_playing_entry_get (xmms_output_t *output, xmms_error_t *err);

#endif
