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

#include "cmdnames.h"

command_name_t *
cmdnames_new (gchar *name)
{
	command_name_t *cmd;

	cmd = g_new0 (command_name_t, 1);
	cmd->name = g_strdup (name);
	cmd->subcommands = NULL;

	return cmd;
}

void
cmdnames_free (GList *list)
{
	GList *it;
	command_name_t *cmd;

	for (it = g_list_first (list); it != NULL; it = g_list_next (it)) {
		cmd = it->data;
		cmdnames_free (cmd->subcommands);
		g_free (cmd->name);
		g_free (cmd);
	}

	g_list_free (list);
}

GList *
cmdnames_prepend (GList *list, gchar **v)
{
	GList *it;
	command_name_t *cmd;

	if (*v == NULL) {
		return list;
	}

	for (it = g_list_first (list); it != NULL; it = g_list_next (it)) {
		cmd = it->data;
		if (!strcmp (*v, cmd->name)) {
			cmd->subcommands = cmdnames_prepend (cmd->subcommands, v+1);
			return list;
		}
	}

	cmd = cmdnames_new (*v);
	cmd->subcommands = cmdnames_prepend (cmd->subcommands, v+1);
	list = g_list_prepend (list, cmd);

	return list;
}

GList *
cmdnames_reverse (GList *list)
{
	GList *it;

	for (it = g_list_first (list); it != NULL; it = g_list_next (it)) {
		command_name_t *cmd = it->data;
		cmd->subcommands = cmdnames_reverse (cmd->subcommands);
	}

	return g_list_reverse (list);
}

GList *
cmdnames_find (GList *list, gchar **v)
{
	GList *it;

	if (v == NULL || *v == NULL) {
		return list;
	}

	for (it = g_list_first (list); it != NULL; it = g_list_next (it)) {
		command_name_t *cmd = it->data;
		if (!strcmp (*v, cmd->name)) {
			return cmdnames_find (cmd->subcommands, v+1);
		}
	}

	return NULL;
}
