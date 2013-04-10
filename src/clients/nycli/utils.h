/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

void print_stats (xmmsv_t *val);
void print_config (cli_infos_t *infos, const gchar *confname);
void print_property (cli_infos_t *infos, xmmsv_t *res, guint id, const gchar *source, const gchar *property);
void remove_ids (cli_infos_t *infos, xmmsv_t *res);
void rehash_ids (cli_infos_t *infos, xmmsv_t *res);
void print_volume (xmmsv_t *dict, const gchar *channel);
void adjust_volume (cli_infos_t *infos, const gchar *channel, gint relative);
void set_volume (cli_infos_t *infos, const gchar *channel, gint volume);
void currently_playing_mode (cli_infos_t *infos, const gchar *format, gint refresh);
void list_print_info (xmmsv_t *res, cli_infos_t *infos);
void list_print_row (xmmsv_t *res, xmmsv_t *filter, column_display_t *coldisp, gboolean is_search, gboolean result_is_infos);
void list_print_playlists (xmmsv_t *res, cli_infos_t *infos, gboolean all);
void list_print_collections (xmmsv_t *res, cli_infos_t *infos);
void list_jump (xmmsv_t *res, cli_infos_t *infos);
void list_jump_back (xmmsv_t *res, cli_infos_t *infos);
void positions_remove (cli_infos_t *infos, const gchar *playlist, playlist_positions_t *positions);
void positions_move (cli_infos_t *infos, const gchar *playlist, playlist_positions_t *positions, gint pos);
void positions_print_info (cli_infos_t *infos, playlist_positions_t *positions);
void positions_print_list (xmmsv_t *res, playlist_positions_t *positions,
                           column_display_t *coldisp, gboolean is_search);
void configure_collection (xmmsv_t *coll, cli_infos_t *infos, const gchar *ns, const gchar *name, const gchar *attrname, const gchar *attrvalue);
void collection_print_config (xmmsv_t *coll, const gchar *attrname);
void coll_save (cli_infos_t *infos, xmmsv_t *coll, xmmsc_coll_namespace_t ns, const gchar *name, gboolean force);
void coll_dump (xmmsv_t *coll, guint level);
void set_next_rel (cli_infos_t *infos, gint offset);
void add_list (xmmsv_t *matching, cli_infos_t *infos, const gchar *playlist, gint pos, gint *count);
void add_recursive (cli_infos_t *infos, const gchar *playlist, const gchar *path, gint pos, gboolean norecurs);
void move_entries (xmmsv_t *matching, xmmsv_t *listvals, cli_infos_t *infos, const gchar *playlist, gint pos);
void remove_cached_list (xmmsv_t *matching, cli_infos_t *infos);
void remove_list (xmmsv_t *matchres, xmmsv_t *plistres, cli_infos_t *infos, const gchar *playlist);
void configure_playlist (xmmsv_t *res, cli_infos_t *infos, const gchar *playlist, gint history, gint upcoming, const gchar *typestr, const gchar *input, const gchar *jumplist);
void playlist_print_config (xmmsv_t *coll, const gchar *name);
gboolean playlist_exists (cli_infos_t *infos, const gchar *playlist);
void print_padding (gint length, const gchar padchar);
void print_indented (const gchar *string, guint level);
gint find_terminal_width (void);
gchar *format_time (guint64 duration, gboolean use_hours);
void enrich_mediainfo (xmmsv_t *val);
gchar *decode_url (const gchar *string);
gchar *format_url (const gchar *path, GFileTest test);

#endif /* __UTILS_H__ */
