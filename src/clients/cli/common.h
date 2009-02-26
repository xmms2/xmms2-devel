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
#ifndef __COMMON_H__
#define __COMMON_H__

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#define CMD_COLL_DEFAULT_NAMESPACE  "Collections"

typedef struct {
	const char *name;
	const char *help;
	void (*func) (xmmsc_connection_t *conn, int argc, char **argv);
} cmds;

gboolean x_realpath (const gchar *item, gchar *rpath);
gchar *x_path2url (gchar *path);
gchar *format_url (gchar *item, GFileTest test);
void print_info (const gchar *fmt, ...);
void print_error (const gchar *fmt, ...);
void print_hash (const gchar *key, xmmsv_t *value, void *udata);
void print_entry (const gchar *key, xmmsv_t *value, void *udata);
gint find_terminal_width ();
void format_pretty_list (xmmsc_connection_t *conn, GList *list);
gint val_has_key (xmmsv_t *val, const gchar *key);
gboolean coll_read_collname (gchar *str, gchar **name, gchar **namespace);
char *string_escape (const char *s);

#endif
