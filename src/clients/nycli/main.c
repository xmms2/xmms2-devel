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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CMD_ARGS 10

gint vrefresh = 0;
gchar *vformat = NULL;

static GOptionEntry entries[] = 
{
  { "refresh", 'r', 0, G_OPTION_ARG_INT, &vrefresh, "Delay between each refresh of the status. If 0, the status is only printed once (default).", "time" },
  { "format",  'f', 0, G_OPTION_ARG_STRING, &vformat, "Format string used to display status.", "format" },
  { NULL }
};

typedef GOptionEntry argument_t;

typedef struct {
	const gchar *name;
 	const argument_t args[MAX_CMD_ARGS + 1];
} command_t;

// FIXME: Store actions
static command_t commands[] =
{
	{ "play", NULL },
	{ "pause", NULL },
	{ "status", {
		{ "refresh", 'r', 0, G_OPTION_ARG_INT, &vrefresh, "Delay between each refresh of the status. If 0, the status is only printed once (default).", "time" },
		{ "format",  'f', 0, G_OPTION_ARG_STRING, &vformat, "Format string used to display status.", "format" },
		{ NULL }
	} },
	{ "quit", NULL },
	{ NULL }
};


// FIXME:
// - type (command, subgroup, etc)
// - callback function
// - completion function
// - connection needed?
typedef struct {
	const gchar *foo;
} command_action_t;

typedef struct {
	gchar c;
	GList* next;
	command_action_t* action;
} command_trie_t;



#define CLI_CLIENTNAME "xmms2-nycli"

typedef enum {
	CLI_EXECUTION_MODE_INLINE,
	CLI_EXECUTION_MODE_SHELL
} execution_mode_t;

typedef struct {
	xmmsc_connection_t *conn;
	execution_mode_t mode;
	command_trie_t *commands;
	GKeyFile *config;
} cli_infos_t;



command_trie_t*
command_trie_alloc () {
	return g_new0 (command_trie_t, 1);
}

command_trie_t*
command_trie_new (char c) {
	command_trie_t* trie = g_new0 (command_trie_t, 1);
	trie->c = c;
	return trie;
}

void
command_trie_free (command_trie_t *trie)
{
	// FIXME: Free the stuff recursively here, kthxbye!
}

gboolean
command_trie_action_set (command_trie_t* node, command_action_t *action)
{
	/* There is already an action registered for that node! */
	if (node->action != NULL) {
		return FALSE;
	} else {
		node->action = action;
		return TRUE;
	}
}

// FIXME: This code sucks
command_trie_t*
command_trie_elem_insert (command_trie_t* node, char c)
{
	GList *prev, *curr;
	command_trie_t *t;

	prev = NULL;
	curr = (GList *)node->next;
	while (curr && ((command_trie_t *)curr->data)->c <= c) {
		prev = curr;
		curr = curr->next;
	}

	if (prev && ((command_trie_t *)prev->data)->c == c) {
		t = (command_trie_t *)prev->data;
	} else {
		t = command_trie_new (c);

		if (prev == NULL) {
			/* List empty so far, bootstrap it */
			node->next = g_list_prepend (curr, t);
		} else {
			/* Insert at the correct position (assign to suppress warning) */
			prev = g_list_insert_before (prev, curr, t);
		}
	}

	return t;
}

gboolean
command_trie_insert (command_trie_t* trie, char *string, command_action_t *action)
{
	const char *c;
	command_trie_t *curr = trie;

	for (c = string; c != NULL; ++c) {
		curr = command_trie_elem_insert (curr, *c);
	}

	return command_trie_action_set (curr, action);
}

void
command_trie_fill (command_trie_t* trie, command_t commands[])
{
	int i;
	const char *c;
	command_t *cmd;
	command_trie_t *curr;

	for (i = 0; commands[i].name != NULL; ++i) {

		curr = trie;
		for (c = commands[i].name; *c != 0; ++c) {
			curr = command_trie_elem_insert (curr, *c);
		}

		// FIXME: Register real action instead!
		command_action_t *action;
		action = g_new (command_action_t, 1);
		action->foo = commands[i].name;

		if (!command_trie_action_set (curr, action)) {
			// FIXME: How to handle error?
			printf ("Error: cannot register action for '%s', already defined!",
			        commands[i].name);
		}
	}
}

gint
command_trie_elem_cmp (gconstpointer elem, gconstpointer udata)
{
	command_trie_t *t;
	const char *c;

	t = (command_trie_t *) elem;
	c = (const char *) udata;

	if (t->c < *c)
		return -1;
	else if (t->c > *c)
		return 1;
	else
		return 0;
}

command_action_t*
command_trie_find (command_trie_t *trie, char *input, char **args)
{
	command_action_t *action;
	GList *l;

	// FIXME: Check for EOS *or* token delimitors, cleanly
	if (*input == 0 || *input == ' ' || *input == '\t') {
		*args = input;
		return trie->action;
	}

	l = g_list_find_custom (trie->next, input, command_trie_elem_cmp);
	if (l == NULL) {
		return NULL;
	}

	return command_trie_find ((command_trie_t *)l->data, input + 1, args);
}


cli_infos_t*
cli_infos_init (gint argc, gchar **argv)
{
	cli_infos_t *infos;
	gint ret;
	gchar *path;

	infos = g_new0 (cli_infos_t, 1);

	infos->commands = command_trie_alloc ();
	command_trie_fill (infos->commands, commands);

	if (argc > 1) {
		infos->mode = CLI_EXECUTION_MODE_SHELL;
	} else {
		infos->mode = CLI_EXECUTION_MODE_INLINE;
	}

	infos->conn = xmmsc_init (CLI_CLIENTNAME);
	if (!infos->conn) {
		// FIXME: nicer way to die?
		printf ("Could not init connection!");
		exit (1);
	}

	char *after;
	command_action_t *action;
	action = command_trie_find (infos->commands, argv[1], &after);
	if (action) {
		printf ("action: %s / after: %s\n", action->foo, after);
	} else {
		printf ("Unknown command: %s\n", argv[1]);
	}

	path = getenv ("XMMS_PATH");
	ret = xmmsc_connect (infos->conn, path);
	if (!ret) {
		// FIXME: first, let's detect whether we need a connection before
		// FIXME: showing an error if we're not connected / unref infos

		// If no connection needed, nothing happens
		// Else
		//   If autostart enabled
		//      Try to start
		//      If success, show message
		//      Else, show error (conn status BAD)
		//   Else
		//      show error (conn status BAD)
	}

	return infos;
}

void
cli_infos_free (cli_infos_t *infos)
{
	xmmsc_unref (infos->conn);
	command_trie_free (infos->commands);
	g_key_file_free (infos->config);
}

gint
main (gint argc, gchar **argv)
{
	gint i;
	GError *error = NULL;
	GOptionContext *context;
	cli_infos_t *cli_infos;

	cli_infos = cli_infos_init (argc, argv);

	// Execute command, if connection status is ok
	if (cli_infos) {

	}

/*
	context = g_option_context_new ("- test args");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	printf("refresh: %d, format: %s\n", vrefresh, vformat);

	for (i = 0; i < argc; ++i) {
		printf("argv[%d]: %s\n", i, argv[i]);
	}

	g_option_context_free (context);
*/
	return 0;
}
