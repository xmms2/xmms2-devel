/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "cli_infos.h"
#include "commands.h"
#include "command_utils.h"
#include "command_trie.h"
#include "configuration.h"
#include "alias.h"

static void alias_run (cli_infos_t *infos, gchar *alias)
{
	gint in_argc;
	gchar **in_argv;

	gchar *next;

	if (!alias || *alias == '\0') {
		return;
	}

	next = strchr (alias, ';');
	*next = '\0';

	g_shell_parse_argv (alias, &in_argc, &in_argv, NULL);

	/* run */
	command_dispatch (infos, in_argc, in_argv);

	alias_run (infos, next + 1);

	g_strfreev (in_argv);
}

static void
free_token (gpointer data, gpointer udata)
{
	g_free (data);
}

static gboolean
runnable_alias (gchar *def, gint argc, gchar **argv, gchar *line, gchar **runnable)
{
	gint k, len;
	gchar **subst;
	gboolean retval = TRUE;

	GList *tokens, *it;

	/* Substitute parameters: $0: line, $@: line, $1: field 1, $2: field 2, ... */
	tokens = alias_tokenize (def);

	len = g_list_length (tokens);

	subst = g_new0 (gchar *, len + 2);
	subst[len] = ";";
	subst[len+1] = NULL;

	if (!line) {
		line = "";
	}

	k = 0;
	for (it = g_list_first (tokens); it != NULL; it = g_list_next (it)) {
		gchar *tok = it->data;
		gint i, size = strlen (tok);

		if (*tok == '$' && strspn (tok, "$@123456789") == size) {
			if (*(tok+1) == '@') {
				i = 0;
			} else {
				i = strtol (tok + 1,  NULL, 10);
			}
			if (argc < i) {
				g_printf ("Error: Invalid alias call (missing parameters)!\n");
				*runnable = NULL;
				retval = FALSE;
				goto finish;
			}
			if (i == 0) {
				subst[k] = line;
			} else {
				subst[k] = argv[i - 1];
			}
		} else {
			subst[k] = tok;
		}
		k++;
	}

	/* put ';' to make parsing simple */
	*runnable = g_strjoinv (" ", subst);

	finish:

	/* free tokens */
	g_list_foreach (tokens, free_token, NULL);
	g_list_free (tokens);

	g_free (subst);

	return retval;;
}

gboolean
alias_action (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *runnable, *def, *line;
	gboolean retval = TRUE;
	gchar **argv;
	gint argc;

	def = g_hash_table_lookup (configuration_get_aliases (infos->config),
	                           command_name_get (ctx));

	argc = command_arg_count (ctx);
	argv = command_argv_get (ctx);

	if (!command_arg_longstring_get_escaped (ctx, 0, &line)) {
		line = NULL;
	}

	if (!runnable_alias (def, argc, argv, line, &runnable)) {
		retval = FALSE;
		goto finish;
	}

	cli_infos_alias_begin (infos);
	alias_run (infos, runnable);
	cli_infos_alias_end (infos);

	finish:
	g_free (line);
	g_free (runnable);

	return retval;
}

GList *
alias_tokenize (const gchar *define)
{
	GList *tokens;
	gchar *tok, *def;

	tokens = NULL;

	def = g_strdup (define);

	/* FIXME: join consecutive strings without '$' char */
	tok = strtok (def, " ");
	while (tok != NULL) {
		tokens = g_list_prepend (tokens, g_strdup (tok));
		tok = strtok (NULL, " ");
	}
	tokens = g_list_reverse (tokens);

	g_free (def);

	return tokens;
}

void
alias_free (alias_define_t *alias)
{
	g_free (alias->name);
	g_free (alias->define);
}

void
alias_setup (command_action_t *action, alias_define_t *alias)
{
	command_action_fill (action, alias->name, &alias_action,
	                     COMMAND_REQ_NONE, NULL,
	                     NULL,
	                     alias->define);
}

static void
hash_to_aliaslist (gpointer name, gpointer define, gpointer udata)
{
       alias_define_t **aliaslist = (alias_define_t **) udata;

       (*aliaslist)->name = g_strdup (name);
       (*aliaslist)->define = g_strdup (define);

       (*aliaslist)++;
}

alias_define_t *
alias_list (GHashTable *hash)
{
	alias_define_t *aliaslist;
	alias_define_t *p;
	guint size;

	size = g_hash_table_size (hash);
	aliaslist = g_new0 (alias_define_t, size + 1);

	p = aliaslist;
	g_hash_table_foreach (hash, hash_to_aliaslist, &p);

	return aliaslist;
}

void
alias_list_free (alias_define_t *list)
{
	gint i;
	for (i = 0; list[i].name; i++) {
		g_free (list[i].name);
		g_free (list[i].define);
	}
	g_free (list);
}
