/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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
#ifndef __CMD_COLL_H__
#define __CMD_COLL_H__

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <glib.h>
#include <string.h>

void cmd_coll (xmmsc_connection_t *conn, gint argc, gchar **argv);

void cmd_coll_save (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_rename (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_list (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_query (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_queryadd (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_find (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_get (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_remove (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_attr (xmmsc_connection_t *conn, gint argc, gchar **argv);
void cmd_coll_sync (xmmsc_connection_t *conn, gint argc, gchar **argv);


#endif
