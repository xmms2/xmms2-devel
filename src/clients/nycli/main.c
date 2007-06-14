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


// dunno what's coming in there really
typedef struct {
	const gchar *foo;
} command_action_t;

typedef struct {
	gchar c;
	GList* next;
	command_action_t* action;
} command_trie_t;


// FIXME: This code sucks
command_trie_t*
command_trie_insert (command_trie_t* node, char c)
{
	command_trie_t *t;
	command_trie_t *curr, *next = NULL;

	curr = (command_trie_t *)node->next;
	while (curr && (next = curr->next) && next->c <= c) {
		curr = (command_trie_t *)curr->next;
	}

	if (curr->c == c) {
		t = curr;
	} else {
		GList *l = g_list_alloc ();
		t = g_new0 (command_trie_t, 1);
		t->c = c;
		l->data = t;
		if (curr) {
			curr->next = l;
		} else {
			node->next = g_list_prepend (node->next, l);
		}
		l->next = next;
	}

	return t;
}

void
command_trie_fill ()
{
	int i;
	const char *c;
	command_trie_t trie;
	command_t *cmd;

	trie.c = 0;
	trie.next = NULL;
	trie.action = NULL;

	for (i = 0; commands[i].name; ++i) {

		command_trie_t *curr = &trie;
		for (c = commands[i].name; c != NULL; ++c) {
			curr = command_trie_insert (curr, *c);
		}

		curr->action = 00;
	}
}

gint
main (gint argc, gchar **argv)
{
	int i;
	GError *error = NULL;
	GOptionContext *context;

	command_trie_fill ();

	if (argc > 1) {
		// shell mode
	} else {
		// inline mode
	}

	context = g_option_context_new ("- test args");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_parse (context, &argc, &argv, &error);

	printf("refresh: %d, format: %s\n", vrefresh, vformat);

	for (i = 0; i < argc; ++i) {
		printf("argv[%d]: %s\n", i, argv[i]);
	}

	g_option_context_free (context);

	return 0;
}
