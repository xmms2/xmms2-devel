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

#include "command_trie.h"


struct command_trie_St {
	gchar c;
	GList* next;
	command_action_t* action;
};

static command_trie_t* command_trie_elem_insert (command_trie_t* node, gchar c);
static gboolean command_trie_action_set (command_trie_t* node, command_action_t *action);
static gint command_trie_elem_cmp (gconstpointer elem, gconstpointer udata);


gint
command_trie_elem_cmp (gconstpointer elem, gconstpointer udata)
{
	command_trie_t *t;
	const gchar *c;

	t = (command_trie_t *) elem;
	c = (const gchar *) udata;

	if (t->c < *c)
		return -1;
	else if (t->c > *c)
		return 1;
	else
		return 0;
}

command_trie_t*
command_trie_alloc ()
{
	return g_new0 (command_trie_t, 1);
}

command_trie_t*
command_trie_new (gchar c)
{
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


void
command_trie_fill (command_trie_t* trie, command_t commands[])
{
	int i;
	const gchar *c;
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

gboolean
command_trie_insert (command_trie_t* trie, gchar *string, command_action_t *action)
{
	const gchar *c;
	command_trie_t *curr = trie;

	for (c = string; c != NULL; ++c) {
		curr = command_trie_elem_insert (curr, *c);
	}

	return command_trie_action_set (curr, action);
}

command_trie_t*
command_trie_elem_insert (command_trie_t* node, gchar c)
{
	GList *prev, *curr;
	command_trie_t *t;

	prev = NULL;
	curr = (GList *) node->next;
	while (curr && ((command_trie_t *) curr->data)->c <= c) {
		prev = curr;
		curr = curr->next;
	}

	if (prev && ((command_trie_t *) prev->data)->c == c) {
		t = (command_trie_t *) prev->data;
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

/* Given a trie node, try to find a unique leaf action, else return NULL. */
command_action_t*
command_trie_find_leaf_action (command_trie_t *trie)
{
	GList *succ;
	command_action_t *action = NULL;

	if (trie) {
		succ = g_list_first (trie->next);
		if (succ && succ->next == NULL && !trie->action) {
			/* No action, a single successor: recurse */
			action = command_trie_find_leaf_action ((command_trie_t *) succ->data);
		} else if (!succ && trie->action) {
			/* No successor, a single action: we found it! */
			action = trie->action;
		}
	}

	return action;
}

command_action_t*
command_trie_find (command_trie_t *trie, gchar *input)
{
	command_action_t *action = NULL;
	GList *l;

	/* FIXME: toggable auto-complete, i.e. if only one matching action, return it */

	if (*input == 0) {
		/* End of token, return current action, or unique completion */
		if (trie->action) {
			action = trie->action;
		} else if (AUTO_UNIQUE_COMPLETE) {
			action = command_trie_find_leaf_action (trie);
		}
	} else {
		/* Recurse in next trie node */
		l = g_list_find_custom (trie->next, input, command_trie_elem_cmp);
		if (l != NULL) {
			action = command_trie_find ((command_trie_t *) l->data, input + 1);
		}
	}

	return action;
}
