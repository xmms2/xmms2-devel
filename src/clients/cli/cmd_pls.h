/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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
#ifndef __CMD_PLS_H__
#define __CMD_PLS_H__

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <glib.h>
#include <string.h>

void cmd_playlist (xmmsc_connection_t *conn, gint argc, gchar **argv);

void cmd_addid (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_add (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_addarg (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_insert (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_insertid (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_radd (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_rinsert (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_clear (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_shuffle (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_sort (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_remove (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_list (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_move (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_addpls (xmmsc_connection_t *conn, gint argc, gchar **argv);

void cmd_playlist_load (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_playlist_create (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_playlist_type (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_playlists_list (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_playlist_active (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_playlist_remove (xmmsc_connection_t *conn, gint argc, gchar **argv);

#endif
