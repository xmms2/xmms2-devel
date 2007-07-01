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

#ifndef __MAIN_H__
#define __MAIN_H__

#define CLI_CLIENTNAME "xmms2-nycli"

/* FIXME: shall be loaded from config when config exists */
#define DEBUG_AUTOSTART TRUE
#define STDINFD 0
#define PROMPT "nycli> "
#define AUTO_UNIQUE_COMPLETE TRUE

/* FIXME: Change this to use gettext later on */
#define _(String) (String)
#define N_(String) String
#define textdomain(Domain)
#define bindtextdomain(Package, Directory)

typedef struct cli_infos_St cli_infos_t;
typedef struct command_trie_St command_trie_t;
typedef struct command_action_St command_action_t;
typedef struct command_context_St command_context_t;
typedef struct command_argument_St command_argument_t;

typedef GOptionEntry argument_t;

typedef void (*command_setup_func)(command_action_t *action);
typedef gboolean (*command_exec_func)(cli_infos_t *infos, command_context_t *ctx);

struct command_context_St {
	gint argc;
	gchar **argv;
	GHashTable *flags;
};

struct command_action_St {
	gchar *name;
	gchar *usage;
	gchar *description;
	command_exec_func callback;
	gboolean req_connection;
	argument_t *argdefs;
};

typedef enum {
	COMMAND_ARGUMENT_TYPE_BOOLEAN = G_OPTION_ARG_NONE,
	COMMAND_ARGUMENT_TYPE_INT = G_OPTION_ARG_INT,
	COMMAND_ARGUMENT_TYPE_STRING = G_OPTION_ARG_STRING
} command_argument_type_t;

typedef union {
	gboolean vbool;
	gint vint;
	gchar *vstring;
} command_argument_value_t;

struct command_argument_St {
	command_argument_type_t type;
	command_argument_value_t value;
};

#endif /* __MAIN_H__ */
