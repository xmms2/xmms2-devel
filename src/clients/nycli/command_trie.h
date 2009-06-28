/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#ifndef __COMMAND_TRIE_H__
#define __COMMAND_TRIE_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>

#include "main.h"

typedef enum {
	COMMAND_TRIE_MATCH_NONE,
	COMMAND_TRIE_MATCH_ACTION,
	COMMAND_TRIE_MATCH_SUBTRIE
} command_trie_match_type_t;

command_trie_t* command_trie_alloc (void);
command_trie_t* command_trie_new (gchar c);
void command_trie_free (command_trie_t *trie);
gboolean command_trie_insert (command_trie_t* trie, command_action_t *action);
command_trie_match_type_t command_trie_find (command_trie_t *trie, gchar ***input, gint *num, gboolean auto_complete, command_action_t **action);

void command_action_fill (command_action_t *action, const gchar *name, command_exec_func cmd, command_req_t req, const argument_t flags[], const gchar *usage, const gchar *description);

command_action_t *command_action_alloc (void);
void command_action_free (command_action_t *action);

#endif /* __COMMAND_TRIE_H__ */
