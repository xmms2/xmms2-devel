/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#include <xmms/xmms_outputplugin.h>
#include <xmmspriv/xmms_playlist.h>
#include <xmmspriv/xmms_plugin.h>

/*
 * Private function prototypes -- do NOT use in plugins.
 */


xmms_output_t *xmms_output_new (xmms_output_plugin_t *plugin, xmms_playlist_t *playlist, xmms_medialib_t *medialib);

void xmms_output_flush (xmms_output_t *output);

/* returns the current latency: time left in ms until the data currently read
 *                              from the latest xform in the chain will actually be played
  */
guint32 xmms_output_latency (xmms_output_t *output);

gboolean xmms_output_plugin_switch (xmms_output_t *output, xmms_output_plugin_t *new_plugin);

#endif
