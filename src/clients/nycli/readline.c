/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#include "readline.h"
#include "configuration.h"

#include "utils.h"
#include "status.h"
#include "cli_infos.h"
#include "cmdnames.h"

#include "command_trie.h"

static gchar *readline_keymap;
static cli_infos_t *readline_cli_infos;

static void
readline_callback (gchar *input)
{
	command_run (readline_cli_infos, input);
}

static void
readline_status_callback (gchar *input)
{
	readline_status_mode_exit ();
}

static gint
readline_status_next (gint count, gint key)
{
	set_next_rel (readline_cli_infos, 1);
	return 0;
}

static gint
readline_status_prev (gint count, gint key)
{
	set_next_rel (readline_cli_infos, -1);
	return 0;
}

static gint
readline_status_toggle (gint count, gint key)
{
	playback_toggle (readline_cli_infos);
	return 0;
}

static int
char_is_quoted (char *text, int index)
{
	return index > 0 && text[index - 1] == '\\';
}

static char *
filename_dequoting (char *text, int quote_char)
{
	/* freed by readline */
	char *unquoted = malloc ((strlen (text) + 1) * sizeof (char));
	char *p = unquoted;

	while (*text) {
		if (*text == '\\') {
			text++;
			continue;
		}
		*p++ = *text++;
	}

	*p = '\0';

	return unquoted;
}


/* Wanted behaviour (no auto-complete):
 * x<TAB> =>
 * <TAB> => [] / add, clear, pause, play, playlist, ...
 *  <TAB> => [ ] / add, clear, pause, play, playlist, ...
 * a<TAB> => [add ]
 * p<TAB> => [p] / pause, play, playlist, etc
 * pa<TAB> => [pause]
 * pla<TAB> => [play] / play, playlist
 * pla <TAB> => [pla ]
 * play<TAB> => [play] / play, playlist
 * playl<TAB> => [playlist ]
 * playlist<TAB> => [playlist ]
 * playlist <TAB> => [playlist ] / clear, config, list, sort, ...
   playlist  <TAB> => [playlist  ] / clear, config, list, sort, ...
 FIXME: ^^^ FAILS currently, need deeper fix in command_trie.c I think
 * playlist c<TAB> => [playlist c] / clear, config
 * playlist clear<TAB> => [playlist clear ]
 * playlist clear <TAB> => [playlist clear ] / <args>
   playlist x<TAB> => [playlist x]
 * add <TAB> => [add ] / <args>
 * play <TAB> => [play ] / <args>
 * play more<TAB> => [play more] / <args>
 * play more <TAB> => [play more ] / <args>
 */

static gchar *
command_tab_completion (const gchar *text, gint state)
{
	static GList *suffixes;
	static gint count;
	static command_trie_match_type_t match;

	/* The first time, compute the list of valid suffixes for the input */
	if (!state) {
		command_action_t *action;
		gchar **args, **tokens;
		gboolean auto_complete;
		gchar *buffer = rl_line_buffer;

		auto_complete = configuration_get_boolean (readline_cli_infos->config,
		                                           "AUTO_UNIQUE_COMPLETE");
		suffixes = NULL;
		while (*buffer == ' ' && *buffer != '\0') ++buffer; /* skip initial spaces */
		args = tokens = g_strsplit (buffer, " ", 0);
		count = g_strv_length (tokens);
		match = command_trie_find (readline_cli_infos->commands, &args, &count,
		                           auto_complete, &action, &suffixes);
		g_strfreev (tokens);
	}

	/* Return each valid suffix, one by one, on subsequent calls */
	while (suffixes) {
		gchar *suffix = NULL, *s;
		gint len;

		s = (gchar *) suffixes->data;
		len = strlen (text) + strlen (s);

		if (len > 0 && (count == 0 || (match == COMMAND_TRIE_MATCH_NONE && count <= 1))) {
			suffix = (gchar *) malloc (len + 1);
			snprintf (suffix, len + 1, "%s%s", text, s);
		}

		g_free (s);
		suffixes = g_list_delete_link (suffixes, suffixes);

		if (suffix != NULL) {
			return suffix;
		}
	}

	return NULL;
}

void
readline_init (cli_infos_t *infos)
{
	readline_cli_infos = infos;
	rl_callback_handler_install (configuration_get_string (infos->config,
	                                                       "PROMPT"),
	                             &readline_callback);

	/* correctly quote filenames with double-quotes */
	rl_filename_quote_characters = " ";
	rl_filename_dequoting_function = filename_dequoting;

	rl_completer_quote_characters = "\"'";
	rl_completer_word_break_characters = (gchar *) " \t\n\"\'";

	rl_char_is_quoted_p = char_is_quoted;
	rl_completion_entry_function = command_tab_completion;
}

void
readline_suspend (cli_infos_t *infos)
{
	rl_callback_handler_remove ();
}

void
readline_resume (cli_infos_t *infos)
{
	rl_callback_handler_install (configuration_get_string (infos->config,
	                                                       "PROMPT"),
	                             &readline_callback);
}

void
readline_status_mode (cli_infos_t *infos)
{
	Keymap stkmap;

	readline_cli_infos = infos;
	rl_callback_handler_install (NULL, &readline_status_callback);

	/* Backup current keymap-name */
	readline_keymap = g_strdup (rl_get_keymap_name (rl_get_keymap ()));

	/* New keymap for status mode */
	stkmap = rl_make_bare_keymap ();

	rl_bind_key_in_map ('\n', rl_newline, stkmap);
	rl_bind_key_in_map ('\r', rl_newline, stkmap);
	rl_bind_key_in_map ('n', readline_status_next, stkmap);
	rl_bind_key_in_map ('p', readline_status_prev, stkmap);
	rl_bind_key_in_map (' ', readline_status_toggle, stkmap);

	rl_set_keymap (stkmap);
}

void
readline_status_mode_exit ()
{
	Keymap active;

	/* Just return if not in status mode */
	if (!readline_cli_infos ||
	    readline_cli_infos->status != CLI_ACTION_STATUS_REFRESH) {
		return;
	}

	active = rl_get_keymap ();

	rl_set_keymap (rl_get_keymap_by_name (readline_keymap));
	rl_discard_keymap (active);

	rl_callback_handler_remove ();
	g_free (readline_keymap);
	status_free (readline_cli_infos->status_entry);
	cli_infos_status_mode_exit (readline_cli_infos);
}

void
readline_free ()
{
	rl_callback_handler_remove ();
}
