/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "main.h"
#include "column_display.h"
#include "playlist_positions.h"

void done (xmmsc_result_t *res, cli_infos_t *infos);
void tickle (xmmsc_result_t *res, cli_infos_t *infos);
void list_plugins (cli_infos_t *infos, xmmsc_result_t *res);
void print_stats (cli_infos_t *infos, xmmsc_result_t *res);
void print_config (cli_infos_t *infos, const gchar *confname);
void print_property (cli_infos_t *infos, xmmsc_result_t *res, guint id, const gchar *source, const gchar *property);
void remove_ids (cli_infos_t *infos, xmmsc_result_t *res);
void rehash_ids (cli_infos_t *infos, xmmsc_result_t *res);
void print_volume (xmmsc_result_t *res, cli_infos_t *infos, const gchar *channel);
void adjust_volume (cli_infos_t *infos, const gchar *channel, gint relative);
void set_volume (cli_infos_t *infos, const gchar *channel, gint volume);
void status_mode (cli_infos_t *infos, const gchar *format, gint refresh);
void list_print_info (xmmsc_result_t *res, cli_infos_t *infos);
void list_print_row (xmmsc_result_t *res, xmmsv_coll_t *filter, column_display_t *coldisp, gboolean is_search);
void list_print_playlists (xmmsc_result_t *res, cli_infos_t *infos, gboolean all);
void list_print_collections (xmmsc_result_t *res, cli_infos_t *infos);
void list_jump (xmmsc_result_t *res, cli_infos_t *infos);
void list_jump_back (xmmsc_result_t *res, cli_infos_t *infos);
void position_jump (cli_infos_t *infos, playlist_positions_t *positions);
void positions_remove (cli_infos_t *infos, const gchar *playlist, playlist_positions_t *positions);
void positions_move (cli_infos_t *infos, const gchar *playlist, playlist_positions_t *positions, gint pos);
void positions_print_info (cli_infos_t *infos, playlist_positions_t *positions);
void positions_print_list (xmmsc_result_t *res, playlist_positions_t *positions,
                           column_display_t *coldisp, gboolean is_search);
void configure_collection (xmmsc_result_t *res, cli_infos_t *infos, const gchar *ns, const gchar *name, const gchar *attrname, const gchar *attrvalue);
void collection_print_config (xmmsc_result_t *res, cli_infos_t *infos, const gchar *attrname);
void coll_rename (cli_infos_t *infos, const gchar *oldname, const gchar *newname, xmmsc_coll_namespace_t ns, gboolean force);
void coll_save (cli_infos_t *infos, xmmsc_coll_t *coll, xmmsc_coll_namespace_t ns, const gchar *name, gboolean force);
void coll_show (cli_infos_t *infos, xmmsc_result_t *res);
void playback_play (cli_infos_t *infos);
void playback_pause (cli_infos_t *infos);
void playback_toggle (cli_infos_t *infos);
void set_next_rel (cli_infos_t *infos, gint offset);
void add_pls (xmmsc_result_t *plsres, cli_infos_t *infos, const gchar *playlist, gint pos);
void add_list (xmmsc_result_t *matching, cli_infos_t *infos, const gchar *playlist, gint pos);
void add_recursive (cli_infos_t *infos, const gchar *playlist, const gchar *path, gint pos, gboolean norecurs);
void move_entries (xmmsc_result_t *matching, cli_infos_t *infos, const gchar *playlist, gint pos);
void remove_cached_list (xmmsc_result_t *matching, cli_infos_t *infos);
void remove_list (xmmsc_result_t *matchres, xmmsc_result_t *plistres, cli_infos_t *infos, const gchar *playlist);
void copy_playlist (xmmsc_result_t *res, cli_infos_t *infos, const gchar *playlist);
void configure_playlist (xmmsc_result_t *res, cli_infos_t *infos, const gchar *playlist, gint history, gint upcoming, xmmsc_coll_type_t type, const gchar *input, const gchar *jumplist);
void playlist_print_config (xmmsc_result_t *res, cli_infos_t *infos, const gchar *playlist);
gboolean playlist_exists (cli_infos_t *infos, const gchar *playlist);
void print_padding (gint length, const gchar padchar);
void print_indented (const gchar *string, guint level);
gint find_terminal_width (void);
gchar *format_time (guint64 duration, gboolean use_hours);
void enrich_mediainfo (xmmsv_t *val);
gchar *decode_url (const gchar *string);
gchar *format_url (const gchar *path, GFileTest test);

#endif /* __UTILS_H__ */
