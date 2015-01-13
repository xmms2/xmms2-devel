/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include <xmmsclient/xmmsclient.h>

#include "cli_context.h"
#include "configuration.h"
#include "readline.h"
#include "status.h"
#include "xmmscall.h"

static gchar *readline_keymap;
static cli_context_t *readline_cli_ctx;

static void
readline_callback (gchar *input)
{
	cli_context_execute_command (readline_cli_ctx, input);
}

static void
readline_status_callback (gchar *input)
{
	readline_status_mode_exit ();
}

static gint
readline_status_quit (gint count, gint key)
{
	/* Let readline_status_callback above be called: */
	return rl_newline (count, key);
}

static gint
readline_next_song (gint count, gint key)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (readline_cli_ctx);
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_set_next_rel, conn, 1),
	                 XMMS_CALL_P (xmmsc_playback_tickle, conn));
	return 0;
}

static gint
readline_previous_song (gint count, gint key)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (readline_cli_ctx);
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_set_next_rel, conn, -1),
	                 XMMS_CALL_P (xmmsc_playback_tickle, conn));
	return 0;
}

static gint
readline_toggle_playback (gint count, gint key)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (readline_cli_ctx);
	guint status = cli_context_playback_status (readline_cli_ctx);

	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		XMMS_CALL (xmmsc_playback_pause, conn);
	} else {
		XMMS_CALL (xmmsc_playback_start, conn);
	}
	return 0;
}

#define create_callback(i) \
static gint \
readline_status_callback##i (gint count, gint key) \
{ \
	return status_call_callback (cli_context_status_entry (readline_cli_ctx), i); \
}

create_callback(0)
create_callback(1)

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
		gchar *buffer = rl_line_buffer;

		suffixes = NULL;
		while (*buffer == ' ' && *buffer != '\0') ++buffer; /* skip initial spaces */
		args = tokens = g_strsplit (buffer, " ", 0);
		count = g_strv_length (tokens);
		match = cli_context_complete_command (readline_cli_ctx, &args, &count,
		                                    &action, &suffixes);
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
readline_init (cli_context_t *ctx)
{
	readline_cli_ctx = ctx;

	/* correctly quote filenames with double-quotes */
	rl_filename_quote_characters = " ";
	rl_filename_dequoting_function = filename_dequoting;

	rl_completer_quote_characters = "\"'";
	rl_completer_word_break_characters = (gchar *) " \t\n\"\'";

	rl_char_is_quoted_p = char_is_quoted;
	rl_completion_entry_function = command_tab_completion;

	/* Set up some named functions for key-bindings */
	rl_add_defun ("next-song", readline_next_song, -1);
	rl_add_defun ("previous-song", readline_previous_song, -1);
	rl_add_defun ("toggle-playback", readline_toggle_playback, -1);
	rl_add_defun ("quit-status-mode", readline_status_quit, -1);
	rl_add_defun ("status-callback-0", readline_status_callback0, -1);
	rl_add_defun ("status-callback-1", readline_status_callback1, -1);
}

void
readline_suspend (cli_context_t *ctx)
{
	rl_callback_handler_remove ();
}

void
readline_resume (cli_context_t *ctx)
{
	configuration_t *config = cli_context_config (ctx);
	rl_callback_handler_install (configuration_get_string (config, "PROMPT"),
	                             &readline_callback);
}

void
readline_status_mode (cli_context_t *ctx, const keymap_entry_t map[])
{
	int i;
	Keymap stkmap;

	readline_cli_ctx = ctx;
	rl_callback_handler_install (NULL, &readline_status_callback);

	/* Backup current keymap-name */
	readline_keymap = g_strdup (rl_get_keymap_name (rl_get_keymap ()));

	/* New keymap for status mode */
	stkmap = rl_make_bare_keymap ();

	/* Fill out the keymap and display it. */
	g_printf ("\n");
	for (i = 0; map[i].keyseq; i++) {
		rl_set_key (map[i].keyseq,
		            rl_named_function (map[i].named_function),
		            stkmap);
		if (map[i].readable_keyseq) {
			g_printf ("   (%s) %s\n", map[i].readable_keyseq,
			          map[i].readable_function ? map[i].readable_function
			                                   : map[i].named_function);
		}
	}
	g_printf ("\n");

	rl_set_keymap (stkmap);
}

void
readline_status_mode_exit (void)
{
	Keymap active;

	g_assert (cli_context_in_status (readline_cli_ctx, CLI_ACTION_STATUS_REFRESH));

	active = rl_get_keymap ();

	rl_set_keymap (rl_get_keymap_by_name (readline_keymap));
	rl_discard_keymap (active);

	rl_callback_handler_remove ();
	g_free (readline_keymap);
	status_free (cli_context_status_entry (readline_cli_ctx)); // TODO: handle via cli_context_free or somesuch?
	cli_context_status_mode_exit (readline_cli_ctx);
}

void
readline_free ()
{
	rl_callback_handler_remove ();
}
