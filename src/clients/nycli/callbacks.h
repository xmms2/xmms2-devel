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

#include <xmmsclient/xmmsclient.h>

#include <glib.h>

#include "main.h"


void cb_done (xmmsc_result_t *res, void *udata);
void cb_tickle (xmmsc_result_t *res, void *udata);
void cb_entry_print_status (xmmsc_result_t *res, void *udata);
void cb_list_print_info (xmmsc_result_t *res, void *udata);
void cb_list_print_row (xmmsc_result_t *res, void *udata);
void cb_list_print_playlists (xmmsc_result_t *res, void *udata);
void cb_list_jump (xmmsc_result_t *res, void *udata);
void cb_list_jump_back (xmmsc_result_t *res, void *udata);
void cb_add_list (xmmsc_result_t *matching, void *udata);
void cb_remove_cached_list (xmmsc_result_t *matching, void *udata);
void cb_remove_list (xmmsc_result_t *matching, xmmsc_result_t *playlist, void *udata);
void cb_copy_playlist (xmmsc_result_t *res, void *udata);
void cb_configure_playlist (xmmsc_result_t *res, void *udata);
void cb_playlist_print_config (xmmsc_result_t *res, void *udata);

#endif /* __CALLBACKS_H__ */
