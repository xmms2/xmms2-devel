/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include "xcu.h"

#include <stdio.h>

#include <glib.h>

#include <command_trie.h>
#include <xmmsc/xmmsc_strlist.h>

static command_trie_t *trie;

static gboolean
cmd_dummy (cli_infos_t *infos, command_t *cmd)
{
	return FALSE;
}

SETUP (command_trie) {
	gint i;

	trie = command_trie_alloc ();

	const gchar *names[] = {
		"play", "pause", "stop",
		"playlist list", "playlist shuffle", "playlist sort"
	};

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		command_action_t *action = command_action_alloc ();
		command_action_fill (action, names[i], cmd_dummy,
		                     COMMAND_REQ_CONNECTION,
		                     NULL, NULL, names[i]);
		command_trie_insert (trie, action);
	}

	return 0;
}

CLEANUP () {
	command_trie_free (trie);
	trie = NULL;
	return 0;
}

CASE (test_match_complete)
{
	command_action_t *action = NULL;
	GList *suffixes = NULL;

	gboolean auto_complete = FALSE;

	gchar **params = xmms_vargs_to_strlist ("playlist", "s", NULL);

	gchar **argv = params;
	gint argc = xmms_strlist_len (argv);

	command_trie_match_type_t result = command_trie_find (trie, &argv, &argc,
	                                                      auto_complete,
	                                                      &action, &suffixes);

	CU_ASSERT_EQUAL (COMMAND_TRIE_MATCH_NONE, result);
	CU_ASSERT_STRING_EQUAL ("huffle", suffixes->data);
	CU_ASSERT_STRING_EQUAL ("ort", suffixes->next->data);
	CU_ASSERT_PTR_NULL (suffixes->next->next);
	CU_ASSERT_PTR_NULL (action);

	g_list_free_full (suffixes, g_free);
	xmms_strlist_destroy (params);
}

CASE (test_match_auto_unique_complete)
{
	command_action_t *action = NULL;
	GList *suffixes = NULL;

	gboolean auto_complete = TRUE;

	gchar **params = xmms_vargs_to_strlist ("playlist", "shuff", NULL);

	gchar **argv = params;
	gint argc = xmms_strlist_len (argv);

	command_trie_match_type_t result = command_trie_find (trie, &argv, &argc,
	                                                      auto_complete,
	                                                      &action, &suffixes);

	CU_ASSERT_EQUAL (COMMAND_TRIE_MATCH_ACTION, result);
	CU_ASSERT_STRING_EQUAL ("le", suffixes->data);
	CU_ASSERT_PTR_NULL (suffixes->next);
	CU_ASSERT_PTR_NOT_NULL (action);
	CU_ASSERT_PTR_NOT_NULL (action->callback);

	g_list_free_full (suffixes, g_free);
	xmms_strlist_destroy (params);
}

CASE (test_match_exact_action)
{
	command_action_t *action = NULL;
	GList *suffixes = NULL;

	gboolean auto_complete = FALSE;

	gchar **params = xmms_vargs_to_strlist ("playlist", "shuffle", NULL);

	gchar **argv = params;
	gint argc = xmms_strlist_len (argv);

	command_trie_match_type_t result = command_trie_find (trie, &argv, &argc,
	                                                      auto_complete,
	                                                      &action, &suffixes);

	CU_ASSERT_EQUAL (COMMAND_TRIE_MATCH_ACTION, result);
	CU_ASSERT_PTR_NULL (suffixes);
	CU_ASSERT_PTR_NOT_NULL (action);
	CU_ASSERT_PTR_NOT_NULL (action->callback);

	xmms_strlist_destroy (params);
}


CASE (test_match_exact_subtrie)
{
	command_action_t *action = NULL;
	GList *suffixes = NULL;

	gboolean auto_complete = FALSE;

	gchar **params = xmms_vargs_to_strlist ("playlist", NULL);

	gchar **argv = params;
	gint argc = xmms_strlist_len (argv);

	command_trie_match_type_t result = command_trie_find (trie, &argv, &argc,
	                                                      auto_complete,
	                                                      &action, &suffixes);

	CU_ASSERT_EQUAL (COMMAND_TRIE_MATCH_SUBTRIE, result);
	CU_ASSERT_PTR_NULL (suffixes);
	CU_ASSERT_PTR_NOT_NULL (action);
	CU_ASSERT_PTR_NULL (action->callback);

	xmms_strlist_destroy (params);
}
