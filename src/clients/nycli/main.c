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

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>

#define MAX_CMD_ARGS 10
#define CLI_CLIENTNAME "xmms2-nycli"

/* FIXME: shall be loaded from config when config exists */
#define DEBUG_AUTOSTART TRUE
#define STDINFD 0
#define PROMPT "nycli> "

typedef struct command_St command_t;
typedef struct command_action_St command_action_t;
typedef struct command_trie_St command_trie_t;
typedef struct command_argument_St command_argument_t;
typedef struct command_context_St command_context_t;
typedef struct cli_infos_St cli_infos_t;

typedef GOptionEntry argument_t;

typedef gboolean (*command_callback_f)(cli_infos_t *infos, command_context_t *ctx);

gboolean cli_play (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pause (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_status (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_quit (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_exit (cli_infos_t *infos, command_context_t *ctx);

void cli_infos_loop_stop (cli_infos_t *infos);

struct command_St {
	const gchar *name;
	command_callback_f callback;
	gboolean req_connection;
	argument_t args[MAX_CMD_ARGS + 1];
};

typedef enum {
	COMMAND_ARGUMENT_TYPE_INT = G_OPTION_ARG_INT,
	COMMAND_ARGUMENT_TYPE_STRING = G_OPTION_ARG_STRING
} command_argument_type_t;

typedef union {
	gchar *vstring;
	gint vint;
} command_argument_value_t;

struct command_argument_St {
	command_argument_type_t type;
	command_argument_value_t value;
};

struct command_context_St {
	gint argc;
	gchar **argv;
	GHashTable *flags;
};


struct command_action_St {
	argument_t *argdefs;
	command_callback_f callback;
	gboolean req_connection;
};

struct command_trie_St {
	gchar c;
	GList* next;
	command_action_t* action;
};

typedef enum {
	CLI_EXECUTION_MODE_INLINE,
	CLI_EXECUTION_MODE_SHELL
} execution_mode_t;

struct cli_infos_St {
	xmmsc_connection_t *conn;
	execution_mode_t mode;
	gboolean running;
	command_trie_t *commands;
	GKeyFile *config;
};


static command_t commands[] =
{
	{ "play", &cli_play, TRUE, NULL },
	{ "pause", &cli_pause, TRUE, NULL },
	{ "status", &cli_status, TRUE, {
		{ "refresh", 'r', 0, G_OPTION_ARG_INT, NULL, "Delay between each refresh of the status. If 0, the status is only printed once (default).", "time" },
		{ "format",  'f', 0, G_OPTION_ARG_STRING, NULL, "Format string used to display status.", "format" },
		{ NULL }
	} },
	{ "quit", &cli_quit, FALSE, NULL },
	{ "exit", &cli_exit, FALSE, NULL },
	{ NULL }
};


/* FIXME: Can we differentiate between unset, set and default value? :-/ */
gboolean
command_flag_int_get (command_context_t *ctx, const gchar *name, gint *v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_INT) {
		*v = arg->value.vint;
		retval = TRUE;
	}

	return retval;
}

gboolean
command_flag_string_get (command_context_t *ctx, const gchar *name, gchar **v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_STRING) {
		*v = arg->value.vstring;
		retval = TRUE;
	}

	return retval;
}

void
DEBUG_dump_flags (gpointer key, gpointer value, gpointer user_data)
{
	gchar *name = (gchar*) key;
	command_argument_t *arg = (command_argument_t*) value;

	if (arg->type == COMMAND_ARGUMENT_TYPE_INT)
		printf ("DEBUG: flag['%s']=%d\n", name, arg->value.vint);
	else
		printf ("DEBUG: flag['%s']='%s'\n", name, arg->value.vstring);
}


gboolean cli_play (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_playback_start (infos->conn);
	return TRUE;
}

gboolean cli_pause (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_playback_pause (infos->conn);
	return TRUE;
}

gboolean cli_status (cli_infos_t *infos, command_context_t *ctx)
{
	command_argument_t *arg;
	gchar *f;
	gint r;

	if (command_flag_int_get (ctx, "refresh", &r)) {
		printf ("refresh=%d\n", r);
	}

	if (command_flag_string_get (ctx, "format", &f)) {
		printf ("format='%s'\n", f);
	}

	return TRUE;
}

gboolean cli_quit (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_quit (infos->conn);
	return TRUE;
}

gboolean cli_exit (cli_infos_t *infos, command_context_t *ctx)
{
	cli_infos_loop_stop (infos);
	return TRUE;
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
	/* FIXME: Free the stuff recursively here, kthxbye! */
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

		command_action_t *action;
		action = g_new0 (command_action_t, 1);

		action->argdefs  = commands[i].args;
		action->callback = commands[i].callback;
		action->req_connection = commands[i].req_connection;
		/* FIXME: when we're done with cb: action->complete = commands[i].complete; */

		if (!command_trie_action_set (curr, action)) {
			/* FIXME: How to handle error? */
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

	/* FIXME: toggable auto-complete, i.e. if only one matching action, return it */

	/* End of token */
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
cli_infos_autostart (cli_infos_t *infos)
{
	return (DEBUG_AUTOSTART && !system ("xmms2-launcher"));
}

void
cli_infos_loop_stop (cli_infos_t *infos)
{
	rl_set_prompt (NULL);
	infos->running = FALSE;
}

/* Called on server disconnection. We can keep the loop running. */
void
cli_infos_disconnect_callback (int flag, void *userdata)
{
	cli_infos_t *infos = (cli_infos_t *) userdata;
	printf ("Server disconnected!\n");
	infos->conn = NULL;
}

gboolean
cli_infos_connect (cli_infos_t *infos)
{
	gchar *path;
	gint ret;

	infos->conn = xmmsc_init (CLI_CLIENTNAME);
	if (!infos->conn) {
		printf ("Could not init connection!\n");
		return FALSE;
	}

	path = getenv ("XMMS_PATH");
	ret = xmmsc_connect (infos->conn, path);
	if (!ret && !cli_infos_autostart (infos)) {
		if (path) {
			printf ("Could not connect to server at '%s'!\n", path);
		} else {
			printf ("Could not connect to server at default path!\n");
		}
		return FALSE;
	}

	xmmsc_ipc_disconnect_set (infos->conn, &cli_infos_disconnect_callback, infos);

	return TRUE;
}

cli_infos_t*
cli_infos_init (gint argc, gchar **argv)
{
	cli_infos_t *infos;

	infos = g_new0 (cli_infos_t, 1);

	if (argc == 0) {
		infos->mode = CLI_EXECUTION_MODE_SHELL;
	} else {
		infos->mode = CLI_EXECUTION_MODE_INLINE;
	}

	infos->running = FALSE;
	infos->commands = command_trie_alloc ();
	command_trie_fill (infos->commands, commands);

	infos->config = g_key_file_new ();

	return infos;
}


void
command_dispatch (cli_infos_t *infos, gint argc, gchar **argv)
{
	gchar *after;
	command_action_t *action;
	action = command_trie_find (infos->commands, *argv);
	if (action) {

		/* FIXME: look at the error! */
		GOptionContext *context;
		GError *error = NULL;
		gint i;

		command_context_t *ctx;
		ctx = g_new0 (command_context_t, 1);

		/* Register a hashtable to receive flag values and pass them on */
		ctx->argc = argc;
		ctx->argv = argv;
		ctx->flags = g_hash_table_new_full (g_str_hash, g_str_equal,
		                                    g_free, g_free); /* FIXME: probably need custom free */

		for (i = 0; action->argdefs[i].long_name; ++i) {
			command_argument_t *arg = g_new (command_argument_t, 1);

			/* FIXME: customizable default values? */
			switch (action->argdefs[i].arg) {
			case G_OPTION_ARG_INT:
				arg->type = COMMAND_ARGUMENT_TYPE_INT;
				arg->value.vint = 0;
				action->argdefs[i].arg_data = &arg->value.vint;
				break;

			case G_OPTION_ARG_STRING:
				arg->type = COMMAND_ARGUMENT_TYPE_STRING;
				arg->value.vstring = NULL;
				action->argdefs[i].arg_data = &arg->value.vstring;
				break;

			default:
				printf ("Trying to register a flag '%s' of invalid type!",
				        action->argdefs[i].long_name);
				break;
			}

			/* FIXME: check for duplicates */
			g_hash_table_insert (ctx->flags,
			                     g_strdup (action->argdefs[i].long_name), arg);
		}

		context = g_option_context_new (NULL);
		g_option_context_set_help_enabled (context, FALSE);  /* runs exit(0)! */
		g_option_context_add_main_entries (context, action->argdefs, NULL);
		g_option_context_parse (context, &ctx->argc, &ctx->argv, &error);
		g_option_context_free (context);

		/* Run action if connection status ok */
		if (!action->req_connection || cli_infos_connect (infos)) {
			action->callback (infos, ctx);
		}
	} else {
		printf ("Unknown command: '%s'\n", *argv);
		printf ("Type 'help' for usage.\n");
	}

}

void
cli_infos_free (cli_infos_t *infos)
{
	if (infos->conn) {
		xmmsc_unref (infos->conn);
	}
	command_trie_free (infos->commands);
	g_key_file_free (infos->config);
}

void
loop_select (cli_infos_t *infos)
{
	fd_set rfds, wfds;
	gint modfds;
	gint xmms2fd;
	gint maxfds = 0;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	/* Listen to xmms2 if connected */
	if (infos->conn) {
		xmms2fd = xmmsc_io_fd_get (infos->conn);
		if (xmms2fd == -1) {
			/* FIXME: Handle error */
		}

		FD_SET(xmms2fd, &rfds);
		if (xmmsc_io_want_out (infos->conn)) {
			FD_SET(xmms2fd, &wfds);
		}

		if (maxfds < xmms2fd) {
			maxfds = xmms2fd;
		}
	}

	/* Listen to readline in shell mode */
	if (infos->mode == CLI_EXECUTION_MODE_SHELL) {
		FD_SET(STDINFD, &rfds);
		if (maxfds < STDINFD) {
			maxfds = STDINFD;
		}
	}

	modfds = select (maxfds + 1, &rfds, &wfds, NULL, NULL);

	if(modfds < 0) {
		/* FIXME: Handle error */
	}
	else if(modfds != 0) {
		/* Get/send data to xmms2 */
		if (infos->conn) {
			if(FD_ISSET(xmms2fd, &rfds)) {
				xmmsc_io_in_handle (infos->conn);
			}
			if(FD_ISSET(xmms2fd, &wfds)) {
				xmmsc_io_out_handle (infos->conn);
			}
		}

		/* User input found, read it */
		if (infos->mode == CLI_EXECUTION_MODE_SHELL
		    && FD_ISSET(STDINFD, &rfds)) {
			rl_callback_read_char ();
		}
	}
}

void
loop_run (cli_infos_t *infos)
{
	infos->running = TRUE;
	while (infos->running) {
		loop_select (infos);
	}
}


cli_infos_t *readline_cli_infos;

void
readline_callback (gchar *input)
{
	while (input && *input == ' ') ++input;

	if (input == NULL) {
		/* End of stream, quit */
		cli_infos_loop_stop (readline_cli_infos);
		printf ("\n");
	} else if (*input != 0) {
		gint argc;
		gchar **argv;
		GError *error;

		if (g_shell_parse_argv (input, &argc, &argv, &error)) {
			add_history (input);
			command_dispatch (readline_cli_infos, argc, argv);
		} else {
			/* FIXME: Handle errors */
		}
	}

}

void
readline_init (cli_infos_t *infos)
{
	readline_cli_infos = infos;
	rl_callback_handler_install (PROMPT, &readline_callback);
}

void
readline_free ()
{
	rl_callback_handler_remove ();
}

gint
main (gint argc, gchar **argv)
{
	gint i;
	GError *error = NULL;
	GOptionContext *context;
	cli_infos_t *cli_infos;

	cli_infos = cli_infos_init (argc - 1, argv + 1);

	/* Execute command, if connection status is ok */
	if (cli_infos) {
		if (cli_infos->mode == CLI_EXECUTION_MODE_INLINE) {
			/* FIXME: Oops, how to "loop once" ? */
			command_dispatch (cli_infos, argc - 1, argv + 1);
		} else {
			readline_init (cli_infos);
			loop_run (cli_infos);
		}
	}

	readline_free ();
	cli_infos_free (cli_infos);

	return 0;
}
