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
void list_plugins (cli_infos_t *infos, xmmsc_result_t *res);
void print_stats (cli_infos_t *infos, xmmsc_result_t *res);
void print_config (cli_infos_t *infos, xmmsc_result_t *res, gchar *confname);
void print_property (cli_infos_t *infos, xmmsc_result_t *res, guint id, gchar *source, gchar *property);
void remove_ids (cli_infos_t *infos, xmmsc_result_t *res);
void rehash_ids (cli_infos_t *infos, xmmsc_result_t *res);
void print_volume (xmmsc_result_t *res, cli_infos_t *infos, gchar *channel);
void set_volume (cli_infos_t *infos, gchar *channel, gint volume);
void status_mode (cli_infos_t *infos, gchar *format, gint refresh);
void list_print_info (xmmsc_result_t *res, cli_infos_t *infos);
void list_print_row (xmmsc_result_t *res, column_display_t *coldisp);
void list_print_playlists (xmmsc_result_t *res, cli_infos_t *infos, gboolean all);
void list_print_collections (xmmsc_result_t *res, cli_infos_t *infos);
void list_jump (xmmsc_result_t *res, cli_infos_t *infos);
void list_jump_back (xmmsc_result_t *res, cli_infos_t *infos);
void configure_collection (xmmsc_result_t *res, cli_infos_t *infos, gchar *ns, gchar *name, gchar *attrname, gchar *attrvalue);
void collection_print_config (xmmsc_result_t *res, cli_infos_t *infos, gchar *attrname);
void coll_rename (cli_infos_t *infos, gchar *oldname, gchar *newname, xmmsc_coll_namespace_t ns, gboolean force);
void coll_save (cli_infos_t *infos, xmmsc_coll_t *coll, xmmsc_coll_namespace_t ns, gchar *name, gboolean force);
void coll_show (cli_infos_t *infos, xmmsc_result_t *res);
void playback_play (cli_infos_t *infos);
void playback_pause (cli_infos_t *infos);
void playback_toggle (cli_infos_t *infos);
void set_next_rel (cli_infos_t *infos, gint offset);
void add_list (xmmsc_result_t *matching, cli_infos_t *infos, gchar *playlist, gint pos);
void add_recursive (cli_infos_t *infos, gchar *playlist, gchar *path, gint pos, gboolean norecurs);
void move_entries (xmmsc_result_t *matching, cli_infos_t *infos, gchar *playlist, gint pos);
void remove_cached_list (xmmsc_result_t *matching, cli_infos_t *infos);
void remove_list (xmmsc_result_t *matchres, xmmsc_result_t *plistres, cli_infos_t *infos, gchar *playlist);
void copy_playlist (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist);
void configure_playlist (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist, gint history, gint upcoming, xmmsc_coll_type_t type, gchar *input);
void playlist_print_config (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist);
gboolean playlist_exists (cli_infos_t *infos, gchar *playlist);

#endif /* __UTILS_H__ */
