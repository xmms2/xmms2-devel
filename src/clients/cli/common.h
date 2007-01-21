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
#ifndef __COMMON_H__
#define __COMMON_H__

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>

#define CMD_COLL_DEFAULT_NAMESPACE  "Collections"

typedef struct {
	char *name;
	char *help;
	void (*func) (xmmsc_connection_t *conn, int argc, char **argv);
} cmds;

gchar *format_url (gchar *item, GFileTest test);
void print_info (const gchar *fmt, ...);
void print_error (const gchar *fmt, ...);
void print_hash (const void *key, xmmsc_result_value_type_t type, 
                 const void *value, void *udata);
void print_entry (const void *key, xmmsc_result_value_type_t type,
                  const void *value, const gchar *source, void *udata);
gint find_terminal_width ();
void format_pretty_list (xmmsc_connection_t *conn, GList *list);
gint res_has_key (xmmsc_result_t *res, const gchar *key);
gboolean coll_read_collname (gchar *str, gchar **name, gchar **namespace);
char *string_escape (const char *s);

#endif
