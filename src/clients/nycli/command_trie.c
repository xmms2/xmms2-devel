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
command_trie_fill (command_trie_t* trie, command_setup_func commandlist[])
{
	int i;
	const gchar *c;
	command_trie_t *curr;

	for (i = 0; commandlist[i] != NULL; ++i) {
		commandlist[i] (trie);
	}
}

static
gboolean argument_copy (const argument_t src[], argument_t **dest)
{
	argument_t *arg;
	int i;

	/* Count elements and allocate size + 1 */
	for (i = 0; src && src[i].long_name; ++i);

	*dest = g_new0 (argument_t, i + 1);

	/* Copy array, last element is all NULL */
	for (i = 0, arg = *dest; src && src[i].long_name; ++i, ++arg) {
		arg->long_name = src[i].long_name;
		arg->short_name = src[i].short_name;
		arg->flags = src[i].flags;
		arg->arg = src[i].arg;
		arg->arg_data = src[i].arg_data;
		arg->description = src[i].description;
		arg->arg_description = src[i].arg_description;
	}

	return TRUE;
}

gboolean
command_trie_insert (command_trie_t* trie, const gchar *name,
                     command_exec_func cmd, gboolean needconn,
                     const argument_t flags[])
{
	const gchar *c;
	command_action_t *action;
	command_trie_t *curr;
	gboolean retval = FALSE;

	action = g_new0 (command_action_t, 1);
	action->name = g_strdup (name);
	action->callback = cmd;
	action->req_connection = needconn;
	if (argument_copy (flags, &action->argdefs)) {
		/* Insert the string in the trie */
		curr = trie;
		for (c = name; *c != 0; ++c) {
			curr = command_trie_elem_insert (curr, *c);
		}

		/* Record the action in the trie node */
	    if (command_trie_action_set (curr, action)) {
			retval = TRUE;
		}
	}

	if (!retval) {
		g_free (action);
	}

	return retval;
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
command_trie_find (command_trie_t *trie, const gchar *input)
{
	command_action_t *action = NULL;
	GList *l;

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
