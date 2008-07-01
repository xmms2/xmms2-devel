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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "main.h"
#include "column_display.h"

void done (xmmsc_result_t *res, cli_infos_t *infos);
void tickle (xmmsc_result_t *res, cli_infos_t *infos);
void entry_print_status (xmmsc_result_t *res, cli_infos_t *infos);
void list_print_info (xmmsc_result_t *res, cli_infos_t *infos);
void list_print_row (xmmsc_result_t *res, column_display_t *coldisp);
void list_print_playlists (xmmsc_result_t *res, cli_infos_t *infos, gboolean all);
void list_jump (xmmsc_result_t *res, cli_infos_t *infos);
void list_jump_back (xmmsc_result_t *res, cli_infos_t *infos);
void add_list (xmmsc_result_t *matching, cli_infos_t *infos, gchar *playlist, gint pos);
void move_entries (xmmsc_result_t *matching, cli_infos_t *infos, gchar *playlist, gint pos);
void remove_cached_list (xmmsc_result_t *matching, cli_infos_t *infos);
void remove_list (xmmsc_result_t *matchres, xmmsc_result_t *plistres, cli_infos_t *infos, gchar *playlist);
void copy_playlist (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist);
void configure_playlist (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist, gint history, gint upcoming, xmmsc_coll_type_t type, gchar *input);
void playlist_print_config (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist);

#endif /* __UTILS_H__ */
