/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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




#ifndef __SIDPLAY_WRAPPER_H
#define __SIDPLAY_WRAPPER_H

#include <glib.h>

struct sidplay_wrapper;
struct sidplay_wrapper *sidplay_wrapper_init(void);
void sidplay_wrapper_destroy(struct sidplay_wrapper *wrap);
const gchar *sidplay_wrapper_md5 (struct sidplay_wrapper *wrap);
void sidplay_wrapper_set_subtune(struct sidplay_wrapper *wrap, gint subtune);
gint sidplay_wrapper_subtunes(struct sidplay_wrapper *wrap);
gint sidplay_wrapper_play(struct sidplay_wrapper *, void *, gint);
gint sidplay_wrapper_load(struct sidplay_wrapper *, void *, gint);
const char *sidplay_wrapper_error(struct sidplay_wrapper *wrap);
gint sidplay_wrapper_songinfo (struct sidplay_wrapper *wrap, gchar *artist, 
							   gchar *title);

#endif
