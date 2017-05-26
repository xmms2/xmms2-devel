/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#ifndef __ALIAS_H__
#define __ALIAS_H__

#include <glib.h>

#include "command_trie.h"

typedef struct alias_define_St alias_define_t;

struct alias_define_St {
	gchar *name;
	gchar *define;
	GList *tokens;
};

void alias_setup (command_action_t *action, alias_define_t *alias);
void alias_free (alias_define_t *alias);
alias_define_t *alias_init (gchar *name, gchar *define);
alias_define_t *alias_list (GHashTable *hash);
void alias_list_free (alias_define_t *list);

#endif /* __ALIAS_H__ */
