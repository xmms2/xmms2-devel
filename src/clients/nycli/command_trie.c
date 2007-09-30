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

typedef enum {
	COMMAND_TRIE_MATCH_NONE,
	COMMAND_TRIE_MATCH_ACTION,
	COMMAND_TRIE_MATCH_SUBTRIE
} command_trie_match_type_t;

typedef struct command_trie_match_St command_trie_match_t;
struct command_trie_match_St {
	command_trie_match_type_t type;
	command_action_t *action;
	command_trie_t *subtrie;
};

struct command_trie_St {
	gchar c;
	GList *next;
	command_trie_match_t match;
};

static command_trie_t* command_trie_elem_insert (command_trie_t* node, gchar c);
static command_trie_t* command_trie_subtrie_insert (command_trie_t* node, gchar c);
static gboolean command_trie_action_set (command_trie_t* node, command_action_t *action);
static gint command_trie_elem_cmp (gconstpointer elem, gconstpointer udata);
static command_action_t* command_trie_find_leaf_action (command_trie_t *trie);


static gint
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

command_action_t *
command_action_alloc ()
{
	return g_new0 (command_action_t, 1);
}

void
command_action_free (command_action_t *action)
{
	g_free (action->name);
	g_free (action->usage);
	g_free (action->description);
	g_free (action->argdefs);
	g_free (action);
}

static void
command_trie_free_with_udata (gpointer elem, gpointer userdata)
{
	command_trie_t *trie = (command_trie_t *) elem;

	command_trie_free (trie);
}


command_trie_t*
command_trie_alloc ()
{
	return g_new0 (command_trie_t, 1);
}

command_trie_t*
command_trie_new (gchar c)
{
	command_trie_t* trie = command_trie_alloc ();
	trie->c = c;
	trie->match.type = COMMAND_TRIE_MATCH_NONE;
	return trie;
}

void
command_trie_free (command_trie_t *trie)
{
	/* Free the trie recursively */
	if (trie->match.type == COMMAND_TRIE_MATCH_ACTION) {
		command_action_free (trie->match.action);
	} else if (trie->match.type == COMMAND_TRIE_MATCH_SUBTRIE) {
		command_trie_free (trie->match.subtrie);
	}
	g_list_foreach (trie->next, command_trie_free_with_udata, NULL);
	g_list_free (trie->next);
	g_free (trie);
}

static gboolean
command_trie_valid_match (command_trie_t* node)
{
	return (node->match.type != COMMAND_TRIE_MATCH_NONE);
}

static gboolean
command_trie_action_set (command_trie_t* node, command_action_t *action)
{
	/* There is already an action registered for that node! */
	if (command_trie_valid_match (node)) {
		return FALSE;
	} else {
		node->match.type = COMMAND_TRIE_MATCH_ACTION;
		node->match.action = action;
		return TRUE;
	}
}

static void
argument_copy (const argument_t src[], argument_t **dest)
{
	argument_t *arg;
	int i;

	/* Count elements and allocate size + 2 (incl. help, NULL) */
	for (i = 0; src && src[i].long_name; ++i);

	*dest = g_new0 (argument_t, i + 2);

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

	/* Add help flag to all commands */
	arg->long_name = "help";
	arg->short_name = 'h';
	arg->flags = 0;
	arg->arg = G_OPTION_ARG_NONE;
	arg->arg_data = NULL;
	arg->description = _("Display command help.");
	arg->arg_description = NULL;
}

static command_trie_t*
command_trie_string_insert (command_trie_t* trie, gchar *name)
{
	command_trie_t *curr;
	gchar *c;

	curr = trie;
	for (c = name; curr && *c != 0; ++c) {
		/* Command separator, enter the subtrie. */
		if (*c == ' ') {
			++c;
			curr = command_trie_subtrie_insert (curr, *c);
		} else {
			curr = command_trie_elem_insert (curr, *c);
		}
	}

	return curr;
}

gboolean
command_trie_insert (command_trie_t* trie, command_action_t *action)
{
	command_trie_t *curr;

	curr = command_trie_string_insert (trie, action->name);
	if (!curr) {
		return FALSE;
	}

	return command_trie_action_set (curr, action);
}

command_action_t*
command_action_fill (command_action_t *action, const gchar *name,
                     command_exec_func cmd, command_req_t req,
                     const argument_t flags[], const gchar *usage,
                     const gchar *description)
{
	action->name = g_strdup (name);
	action->usage = g_strdup (usage);
	action->description = g_strdup (description);
	action->callback = cmd;
	action->req = req;
	argument_copy (flags, &action->argdefs);
}

static command_trie_t*
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

static command_trie_t*
command_trie_subtrie_insert (command_trie_t* node, gchar c)
{
	if (node->match.type == COMMAND_TRIE_MATCH_ACTION) {
		/* Cannot overwrite an existing action, error! */
		return NULL;
	} else if (node->match.type == COMMAND_TRIE_MATCH_NONE) {
		node->match.type = COMMAND_TRIE_MATCH_SUBTRIE;
		node->match.subtrie = command_trie_alloc ();
	}

	return command_trie_elem_insert (node->match.subtrie, c);
}

/* Given a trie node, try to find a unique leaf action, else return NULL. */
static command_action_t*
command_trie_find_leaf_action (command_trie_t *trie)
{
	GList *succ;
	command_action_t *action = NULL;

	if (trie) {
		succ = g_list_first (trie->next);
		if (succ && succ->next == NULL && !command_trie_valid_match (trie)) {
			/* Not a matching node, a single successor: recurse */
			action = command_trie_find_leaf_action ((command_trie_t *) succ->data);
		} else if (!succ && command_trie_valid_match (trie)) {
			/* No successor, matching node: we found it! */
			if (trie->match.type == COMMAND_TRIE_MATCH_SUBTRIE) {
				action = command_trie_find_leaf_action (trie->match.subtrie);
			} else if (trie->match.type == COMMAND_TRIE_MATCH_ACTION) {
				action = trie->match.action;
			}
		}
	}

	return action;
}

command_action_t*
command_trie_find (command_trie_t *trie, const gchar *input,
                   gboolean auto_complete)
{
	command_action_t *action = NULL;
	GList *l;

	/* FIXME: support the subtrie */
	if (*input == ' ') {
		/* End of token, return current action, or unique completion */
		if (trie->match.type == COMMAND_TRIE_MATCH_ACTION) {
			action = trie->match.action;
		} else if (trie->match.type == COMMAND_TRIE_MATCH_SUBTRIE) {
			action = command_trie_find (trie->match.subtrie, input + 1, auto_complete);
		}
		/* FIXME: do something with auto-complete here */
	} else if (*input == 0) {
		/* End of token, return current action, or unique completion */
		if (trie->match.type == COMMAND_TRIE_MATCH_ACTION) {
			action = trie->match.action;
		} else if (auto_complete) {
			action = command_trie_find_leaf_action (trie);
		}
	} else {
		/* Recurse in next trie node */
		l = g_list_find_custom (trie->next, input, command_trie_elem_cmp);
		if (l != NULL) {
			action = command_trie_find ((command_trie_t *) l->data, input + 1,
			                             auto_complete);
		}
	}

	return action;
}
