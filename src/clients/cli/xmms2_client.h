/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

typedef struct {
	char *name;
	char *help;
	void (*func) (xmmsc_connection_t *conn, int argc, char **argv);
} cmds;

void print_info (const char *fmt, ...);
char *format_url (char *item);
void print_error (const char *fmt, ...);
void print_hash (const void *key, xmmsc_result_value_type_t type, const void *value, void *udata);
void format_pretty_list (xmmsc_connection_t *conn, GList *list);

void cmd_mlib (xmmsc_connection_t *conn, int argc, char **argv);
