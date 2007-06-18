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


typedef enum {
	COMMAND_ARGUMENT_TYPE_INT = G_OPTION_ARG_INT,
	COMMAND_ARGUMENT_TYPE_STRING = G_OPTION_ARG_STRING
} command_argument_type_t;

typedef union {
	gchar *vstring;
	gint vint;
} command_argument_value_t;

typedef struct {
	command_argument_type_t type;
	command_argument_value_t value;
} command_argument_t;

typedef struct {
	gint argc;
	gchar **argv;
	GHashTable *flags;
} command_context_t;

typedef struct cli_infos_St cli_infos_t;

// FIXME: ideally, put all the stuff in a context struct
typedef gboolean (*command_action_callback_f)(cli_infos_t *infos, command_context_t *ctx);

// FIXME:
// - type (command, subgroup, etc)
// OR
// - callback function
// - completion function
typedef struct {
	const argument_t *argdefs;
	command_action_callback_f callback;
	command_action_callback_f complete;
	gboolean req_connection;
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

struct cli_infos_St {
	xmmsc_connection_t *conn;
	execution_mode_t mode;
	command_trie_t *commands;
	GKeyFile *config;
};


gboolean
command_argument_cmp (gconstpointer a, gconstpointer b)
{
	command_argument_t *v1;
	command_argument_t *v2;
	gboolean retval = FALSE;

	v1 = (command_argument_t*) a;
	v2 = (command_argument_t*) b;

	if (v1->type == v2->type) {
		switch (v1->type) {
		case COMMAND_ARGUMENT_TYPE_INT:
			if (v1->value.vint == v2->value.vint) {
				retval = TRUE;
			}
			break;

		case COMMAND_ARGUMENT_TYPE_STRING:
			if (strcmp (v1->value.vstring, v2->value.vstring) == 0) {
				retval = TRUE;
			}
			break;

		default:
			break;
		}
	}

	return retval;
}

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


gboolean
sugoi(cli_infos_t *infos, command_context_t *ctx)
{
	printf ("korv\n");
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
		action = g_new0 (command_action_t, 1);

		// FIXME: Fake!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		action->argdefs = commands[i].args;
		action->callback = &sugoi;

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
command_trie_find (command_trie_t *trie, char *input)
{
	command_action_t *action;
	GList *l;

	// FIXME: toggable auto-complete, i.e. if only one matching action, return it

	// End of token
	if (*input == 0) {
		return trie->action;
	}

	l = g_list_find_custom (trie->next, input, command_trie_elem_cmp);
	if (l == NULL) {
		return NULL;
	}

	return command_trie_find ((command_trie_t *)l->data, input + 1);
}


gboolean
cli_infos_connection (cli_infos_t *infos)
{
	gchar *path;
	gint ret;

	infos->conn = xmmsc_init (CLI_CLIENTNAME);
	if (!infos->conn) {
		printf ("Could not init connection!");
		return FALSE;
	}

	path = getenv ("XMMS_PATH");
	ret = xmmsc_connect (infos->conn, path);
	if (!ret) {
		if (path) {
			printf ("Could not connect to server at '%s'!", path);
		} else {
			printf ("Could not connect to server at default path!");
		}
		return FALSE;
	}

	return TRUE;
}

cli_infos_t*
cli_infos_init (gint argc, gchar **argv)
{
	cli_infos_t *infos;

	infos = g_new0 (cli_infos_t, 1);

	infos->commands = command_trie_alloc ();
	command_trie_fill (infos->commands, commands);

	if (argc > 1) {
		infos->mode = CLI_EXECUTION_MODE_SHELL;
	} else {
		infos->mode = CLI_EXECUTION_MODE_INLINE;
	}

	// FIXME: if shell mode, tokenize with g_shell_parse_argv ?

	gchar *after;
	command_action_t *action;
	action = command_trie_find (infos->commands, argv[1]);
	if (action) {
		// FIXME: Call the action later on
		gint newargc = argc - 1;
		gchar **newargv = argv + 1;

		command_context_t *ctx;
		ctx = g_new0 (command_context_t, 1);

		// FIXME: look at the error!
		GError *error = NULL;
		GOptionContext *context;
		gint i;

		context = g_option_context_new ("- test args");
		g_option_context_add_main_entries (context, action->argdefs, NULL);
		g_option_context_parse (context, &newargc, &newargv, &error);

		// FIXME: Find a way to put the arguments into a struct, and save it in flags

		ctx->argc = newargc;
		ctx->argv = newargv;
		ctx->flags = g_hash_table_new (g_str_hash, command_argument_cmp);

		// FIXME: it's a bit shitty, couldn't we have a custom arg struct for each callback?
		//   Uglier to use a hashtable, autogen struct, or autogen the function prototype?
		//   In the latter case, what are the default arguments? explicitely specified?
		//   Can we really also autogen the call of the callback... ?
		//   Or.. doh.. vargs?
		//   Or define arg_{int,string,bool}_get(name) applicable on some data structure?
		//   All this assumes we recreated the context parser everytime (until optims arrive)
		printf("refresh: %d, format: %s\n", vrefresh, vformat);

		// FIXME:
		// For all options in array
		//   create a new arg struct
		//   register its memory location in the array
		//   add it to the hash
		// Register argdefs in the option_context
		// -note- this could be done the first time for all commands, maybe using groups?
		for (i = 0; action->argdefs[i]; ++i) {
			command_argument_t *arg = g_new (command_argument_t, 1);

			switch (action->argdefs[i]) {
			case G_OPTION_ARG_INT:
				arg->type = COMMAND_ARGUMENT_TYPE_INT;
				break;

			case G_OPTION_ARG_STRING:
				arg->type = COMMAND_ARGUMENT_TYPE_STRING;
				break;

			default:
				// FIXME: Other possible types?
				break;
			}

		}

		for (i = 0; i < newargc; ++i) {
			printf("argv[%d]: %s\n", i, newargv[i]);
		}

		g_option_context_free (context);

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

		action->callback (infos, ctx);
	} else {
		printf ("Unknown command: %s\n", argv[1]);
	}


	return infos;
}

void
command_dispatch (cli_infos_t *infos, gint argc, gchar **argv)
{

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
		if (cli_infos->mode == CLI_EXECUTION_MODE_INLINE) {
			command_dispatch (cli_infos, argc, argv);
		} else {
			// FIXME: start a loop, init readline and stuff
		}
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
