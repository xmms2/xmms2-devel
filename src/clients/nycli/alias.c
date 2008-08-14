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

#include "cli_infos.h"
#include "commands.h"
#include "command_utils.h"
#include "command_trie.h"
#include "alias.h"

gboolean
alias_run (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *name;

	command_arg_string_get (ctx, 0, &name);
	g_printf ("run alias %s\n", name);

	return TRUE;
}

void
alias_setup (command_action_t *action, alias_define_t *alias)
{
	command_action_fill (action, alias->name, &alias_run,
	                     COMMAND_REQ_CONNECTION, NULL,
	                     NULL,
	                     alias->define);
}

alias_define_t **
alias_list (GHashTable *hash)
{
	alias_define_t **aliaslist;
	gpointer name, define;
	guint i, size;

	GHashTableIter it;

	size = g_hash_table_size (hash);
	aliaslist = g_new0 (alias_define_t *, size + 1);

	i = 0;
	g_hash_table_iter_init (&it, hash);
	while (g_hash_table_iter_next (&it, &name, &define)) {
		alias_define_t *alias;
	
		alias = g_new0 (alias_define_t, 1);
		alias->name = g_strdup (name);
		alias->define = g_strdup (define);

		aliaslist[i++] = alias;
	}
	
	return aliaslist;
}

void
alias_list_free (alias_define_t **list)
{
	int i;

	for (i = 0; list[i] != NULL; i++) {
		g_free (list[i]);
	}
	g_free (list);
}
