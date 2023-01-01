/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#ifndef __CMDNAMES_H__
#define __CMDNAMES_H__

#include <string.h>

#include <glib.h>

typedef struct command_name_St command_name_t;
struct command_name_St {
	gchar *name;
	GList *subcommands;
};

command_name_t* cmdnames_new (gchar *name);
void cmdnames_free (GList *list);
GList* cmdnames_prepend (GList *list, gchar **v);
GList* cmdnames_reverse (GList *list);
GList* cmdnames_find (GList *list, gchar **v);

#endif /* __CMDNAMES_H__ */
