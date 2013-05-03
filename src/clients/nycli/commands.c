/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include "cli_infos.h"
#include "cmdnames.h"
#include "commands.h"
#include "command_utils.h"
#include "utils.h"

#define COMMAND_HELP_DESCRIPTION_INDENT 2

/* Setup commands */

#define CLI_SIMPLE_SETUP(name, cmd, req, usage, desc) \
	void \
	cmd##_setup (command_action_t *action) \
	{ command_action_fill (action, name, cmd, req, NULL, usage, desc); }

CLI_SIMPLE_SETUP("play", cli_play,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("Start playback."))

CLI_SIMPLE_SETUP("pause", cli_pause,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("Pause playback."))

CLI_SIMPLE_SETUP("toggle", cli_toggle, /* <<<<< */
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE,
                 NULL,
                 _("Toggle playback."))

CLI_SIMPLE_SETUP("stop", cli_stop,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("Stop playback."))

CLI_SIMPLE_SETUP("seek", cli_seek,
                 COMMAND_REQ_CONNECTION,
                 _("<time|offset>"),
                 _("Seek to a relative or absolute position."))

CLI_SIMPLE_SETUP("prev", cli_prev,
                 COMMAND_REQ_CONNECTION,
                 _("[offset]"),
                 _("Jump to previous song."))

CLI_SIMPLE_SETUP("next", cli_next,
                 COMMAND_REQ_CONNECTION,
                 _("[offset]"),
                 _("Jump to next song."))

CLI_SIMPLE_SETUP("info", cli_info,
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE,
                 _("<pattern|positions>"),
                 _("Display all the properties for all media matching the pattern."))

CLI_SIMPLE_SETUP("exit", cli_exit,
                 COMMAND_REQ_NONE,
                 NULL,
                 _("Exit the shell-like interface."))

CLI_SIMPLE_SETUP("playlist switch", cli_pl_switch,
                 COMMAND_REQ_CONNECTION,
                 _("<playlist>"),
                 _("Change the active playlist."))

CLI_SIMPLE_SETUP("playlist remove", cli_pl_remove,
                 COMMAND_REQ_CONNECTION,
                 _("<playlist>"),
                 _("Remove the given playlist."))

CLI_SIMPLE_SETUP("playlist clear", cli_pl_clear,
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE,
                 _("[playlist]"),
                 _("Clear a playlist.  By default, clear the active playlist."))

CLI_SIMPLE_SETUP("playlist shuffle", cli_pl_shuffle,
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE,
                 _("[playlist]"),
                 _("Shuffle a playlist.  By default, shuffle the active playlist."))

CLI_SIMPLE_SETUP("collection list", cli_coll_list,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("List all collections."))

CLI_SIMPLE_SETUP("collection show", cli_coll_show,
                 COMMAND_REQ_CONNECTION,
                 _("<collection>"),
                 _("Display a human-readable description of a collection."));

CLI_SIMPLE_SETUP("collection remove", cli_coll_remove,
                 COMMAND_REQ_CONNECTION,
                 _("<collection>"),
                 _("Remove a collection."))

CLI_SIMPLE_SETUP("collection config", cli_coll_config,
                 COMMAND_REQ_CONNECTION,
                 _("<collection> [attrname [attrvalue]]"),
                 _("Get or set attributes for the given collection.\n"
                   "If no attribute name is provided, list all attributes.\n"
                   "If only an attribute name is provided, display the value of the attribute.\n"
                   "If both attribute name and value are provided, set the new value of the attribute."))

CLI_SIMPLE_SETUP("server browse", cli_server_browse,
                 COMMAND_REQ_CONNECTION,
                 _("<url>"),
                 _("Browse a resource location."))

CLI_SIMPLE_SETUP("server remove", cli_server_remove,
                 COMMAND_REQ_CONNECTION,
                 _("<pattern>"),
                 _("Remove the matching media from the media library."))

CLI_SIMPLE_SETUP("server rehash", cli_server_rehash,
                 COMMAND_REQ_CONNECTION,
                 _("[pattern]"),
                 _("Rehash the media matched by the pattern,\n"
                   "or the whole media library if no pattern is provided"))

CLI_SIMPLE_SETUP("server config", cli_server_config,
                 COMMAND_REQ_CONNECTION,
                 _("[name [value]]"),
                 _("Get or set configuration values.\n"
                   "If no name or value is provided, list all configuration values.\n"
                   "If only a name is provided, display the content of the corresponding configuration value.\n"
                   "If both name and a value are provided, set the new content of the configuration value."))

CLI_SIMPLE_SETUP("server plugins", cli_server_plugins,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("List the plugins loaded in the server."))

CLI_SIMPLE_SETUP("server stats", cli_server_stats,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("Display statistics about the server: uptime, version, size of the medialib, etc"))

CLI_SIMPLE_SETUP("server sync", cli_server_sync,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("Force the saving of collections to the disk (otherwise only performed on shutdown)"))

CLI_SIMPLE_SETUP("server shutdown", cli_server_shutdown,
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_NO_AUTOSTART,
                 NULL,
                 _("Shutdown the server."))

void
cli_help_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "alias", 'a', 0, G_OPTION_ARG_NONE, NULL, _("List aliases, or alias definition."), NULL },
		{ NULL }
	};
	command_action_fill (action, "help", &cli_help, COMMAND_REQ_NONE, flags,
	                     _("[-a] [command]"),
	                     _("List all commands, or help on one command."));
}

void
cli_jump_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "backward", 'b', 0, G_OPTION_ARG_NONE, NULL, _("Jump backward to the first media matching the pattern"), NULL },
		{ NULL }
	};
	command_action_fill (action, "jump", &cli_jump, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-b] <pattern|positions>"),
	                     _("Jump to the first media matching the pattern."));
}

void
cli_search_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "order",   'o', 0, G_OPTION_ARG_STRING, NULL, _("List of properties to order by (prefix by '-' for reverse ordering)."), "prop[,...]" },
		{ "columns", 'l', 0, G_OPTION_ARG_STRING, NULL, _("List of properties to use as columns."), "prop[,...]" },
		{ NULL }
	};
	command_action_fill (action, "search", &cli_search, COMMAND_REQ_CONNECTION, flags,
	                     _("[-o <prop[,...]>] [-l <prop[,...]>] <pattern>"),
	                     _("Search and print all media matching the pattern."));
}

void
cli_list_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "playlist",   'p', 0, G_OPTION_ARG_STRING, NULL, _("List the given playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "list", &cli_list, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-p <name>] <pattern|position>"),
	                     _("List the contents of a playlist (the active playlist by default). If a\n"
	                       "pattern is provided, contents are further filtered and only the matching\n"
	                       "media are displayed."));
}

void
cli_add_setup (command_action_t *action)
{
	/* FIXME: support collection ? */
	const GOptionEntry flags[] = {
		{ "file", 'f', 0, G_OPTION_ARG_NONE, NULL, _("Treat the arguments as file paths instead of a pattern."), "path" },
		{ "pls", 'P', 0, G_OPTION_ARG_NONE, NULL, _("Treat the files as playlist files (implies --file.)"), "path" },
		{ "pattern", 't', 0, G_OPTION_ARG_NONE, NULL, _("Force treating arguments as pattern."), "pattern" },
		{ "non-recursive", 'N', 0, G_OPTION_ARG_NONE, NULL, _("Do not add directories recursively."), NULL },
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Add to the given playlist."), "name" },
		{ "next", 'n', 0, G_OPTION_ARG_NONE, NULL, _("Add after the current track."), NULL },
		{ "at", 'a', 0, G_OPTION_ARG_INT, NULL, _("Add media at a given position in the playlist, or at a given offset from the current track."), "pos|offset" },
		{ "attribute", 'A', 0, G_OPTION_ARG_STRING_ARRAY, NULL, _("Add media with given key=value attribute(s)."), NULL },
		{ "order", 'o', 0, G_OPTION_ARG_STRING, NULL, _("Order media by specified properties."), NULL },
		{ "jump", 'j', 0, G_OPTION_ARG_NONE, NULL, _("Jump to and start playing the newly-added media."), NULL },
		{ NULL }
	};
	command_action_fill (action, "add", &cli_add, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-t | -f [-N] [-P] [-A key=value]... ] [-p <playlist>] [-n | -a <pos|offset>] [-j] [pattern | paths] -o prop[,...]"),
	                     _("Add the matching media or files to a playlist."));
}

void
cli_remove_setup (command_action_t *action)
{
	/* FIXME: support collection ? */
	const GOptionEntry flags[] = {
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Remove from the given playlist, instead of the active playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "remove", &cli_remove, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-p <playlist>] <pattern|positions>"),
	                     _("Remove the matching media from a playlist."));
}

void
cli_move_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Playlist to act on."), "name" },
		{ "next", 'n', 0, G_OPTION_ARG_NONE, NULL, _("Move the matching tracks after the current track."), NULL },
		{ "at", 'a', 0, G_OPTION_ARG_INT, NULL, _("Move the matching tracks by an offset or to a position."), "pos|offset"},
		{ NULL }
	};
	command_action_fill (action, "move", &cli_move, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-p <playlist>] [-n | -a <pos|offset>] <pattern|positions>"),
	                     _("Move entries inside a playlist."));
}

void
cli_current_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "refresh", 'r', 0, G_OPTION_ARG_INT, NULL, _("Delay between each refresh of the status. If 0, the status is only printed once (default)."), "time" },
		{ "format",  'f', 0, G_OPTION_ARG_STRING, NULL, _("Format string used to display status."), "format" },
		{ NULL }
	};
	command_action_fill (action, "current", &cli_current, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-r <time>] [-f <format>]"),
	                     _("Display current playback status, either continuously or once."));
}

void
cli_pl_create_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "switch", 's', 0, G_OPTION_ARG_NONE, NULL, _("Switch to the newly created playlist."), NULL },
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Copy the content of the playlist into the new playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "playlist create", &cli_pl_create, COMMAND_REQ_CONNECTION, flags,
	                     _("[-s] [-p <playlist>] <name>"),
	                     _("Create a new playlist."));
}

void
cli_pl_rename_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "force", 'f', 0, G_OPTION_ARG_NONE, NULL, _("Force the rename of the collection, overwrite an existing collection if needed."), NULL },
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Rename the given playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "playlist rename", &cli_pl_rename, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-f] [-p <playlist>] <newname>"),
	                     _("Rename a playlist.  By default, rename the active playlist."));
}

void
cli_pl_sort_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Rename the given playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "playlist sort", &cli_pl_sort, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-p <playlist>] [prop] ..."),
	                     _("Sort a playlist by a list of properties.  By default, sort the active playlist.\n"
	                        "To sort by a property in reverse, prefix its name by a '-'."));
}

void
cli_coll_create_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "force", 'f', 0, G_OPTION_ARG_NONE, NULL, _("Force creating of the collection, overwrite an existing collection if needed."), NULL},
		{ "collection", 'c', 0, G_OPTION_ARG_STRING, NULL, _("Copy an existing collection to the new one."), "name"},
		{ "empty", 'e', 0, G_OPTION_ARG_NONE, NULL, _("Initialize an empty collection."), NULL},
		{ NULL }
	};
	command_action_fill (action, "collection create", &cli_coll_create, COMMAND_REQ_CONNECTION, flags,
	                     _("[-f] [-e] [-c <collection>] <name> [pattern]"),
	                     _("Create a new collection.\nIf pattern is provided, it is used to define the collection."
	                       "\nOtherwise, the new collection contains the whole media library."));
}

void
cli_coll_rename_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "force", 'f', 0, G_OPTION_ARG_NONE, NULL, _("Force renaming of the collection, overwrite an existing collection if needed."), NULL},
		{ NULL }
	};
	command_action_fill (action, "collection rename", &cli_coll_rename, COMMAND_REQ_CONNECTION, flags,
	                     _("[-f] <oldname> <newname>"),
	                     _("Rename a collection."));
}

void
cli_pl_config_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "type",    't', 0, G_OPTION_ARG_STRING, NULL, _("Change the type of the playlist: list, queue, pshuffle."), "type" },
		{ "history", 's', 0, G_OPTION_ARG_INT, NULL, _("Size of the history of played tracks (for queue, pshuffle)."), "n" },
		{ "upcoming",'u', 0, G_OPTION_ARG_INT, NULL, _("Number of upcoming tracks to maintain (for pshuffle)."), "n" },
		{ "input",   'i', 0, G_OPTION_ARG_STRING, NULL, _("Input collection for the playlist (for pshuffle). Default to 'All Media'."), "coll" },
		{ "jumplist",'j', 0, G_OPTION_ARG_STRING, NULL, _("Jump to another playlist when the end of the playlist is reached."), "playlist"},
		{ NULL }
	};
	command_action_fill (action, "playlist config", &cli_pl_config, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-t <type>] [-s <history>] [-u <upcoming>] [-i <coll>] [-j <playlist>] [playlist]"),
	                     _("Configure a playlist by changing its type, attributes, etc.\nBy default, configure the active playlist."));
}

void
cli_pl_list_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "all",  'a', 0, G_OPTION_ARG_NONE, NULL, _("Include hidden playlists."), NULL },
		{ NULL }
	};
	command_action_fill (action, "playlist list", &cli_pl_list, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-a]"),
	                     _("List all playlists."));
}

void
cli_server_import_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "non-recursive", 'N',  0, G_OPTION_ARG_NONE, NULL, _("Do not import directories recursively."), NULL },
		{ NULL }
	};
	command_action_fill (action, "server import", &cli_server_import, COMMAND_REQ_CONNECTION, flags,
	                     _("[-N] <path>"),
	                     _("Import new files into the media library.\n"
	                     "By default, directories are imported recursively."));
}

void
cli_server_property_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "int",    'i',  0, G_OPTION_ARG_NONE, NULL, _("Force the value to be treated as integer."), NULL },
		{ "string", 's',  0, G_OPTION_ARG_NONE, NULL, _("Force the value to be treated as a string."), NULL },
		{ "delete", 'D',  0, G_OPTION_ARG_NONE, NULL, _("Delete the selected property."), NULL },
		{ "source", 'S',  0, G_OPTION_ARG_STRING, NULL, _("Property source."), NULL },
		{ NULL }
	};
	command_action_fill (action, "server property", &cli_server_property, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-i | -s | -D] [-S] <mid> [name [value]]"),
	                     _("Get or set properties for a given media.\n"
	                     "If no name or value is provided, list all properties.\n"
	                     "If only a name is provided, display the value of the property.\n"
	                     "If both a name and a value are provided, set the new value of the property.\n\n"
	                     "By default, set operations use source \"client/" CLI_CLIENTNAME "\", while list and display operations use source-preference.\n"
	                     "Use the --source option to override this behaviour.\n\n"
	                     "By default, the value will be used to determine whether it should be saved as a string or an integer.\n"
	                     "Use the --int or --string flag to override this behaviour."));
}

void
cli_server_volume_setup (command_action_t *action)
{
	const GOptionEntry flags[] = {
		{ "channel", 'c',  0, G_OPTION_ARG_STRING, NULL, _("Get or set the volume only for one channel."), "name" },
		{ NULL }
	};
	command_action_fill (action, "server volume", &cli_server_volume, COMMAND_REQ_CONNECTION, flags,
	                     _("[-c <name>] [value]"),
	                     _("Get or set the audio volume (in a range of 0-100).\n"
	                     "If a value is provided, set the new value of the volume. Otherwise, display the current volume.\n"
	                     "By default, the command applies to all audio channels. Use the --channel flag to override this behaviour."));
}

/* Define commands */

gboolean
cli_exit (cli_infos_t *infos, command_context_t *ctx)
{
	cli_infos_loop_stop (infos);
	return FALSE;
}

static void
help_short_command (gpointer elem, gpointer udata)
{
	command_name_t *cmd = (command_name_t *) elem;
	if (cmd->subcommands) {
		g_printf ("   %s <subcommand>\n", cmd->name);
	} else {
		g_printf ("   %s\n", cmd->name);
	}
}

static void
help_list_commands (GList *names,
                    const gchar *sing, const gchar *plur, const gchar *det)
{
	g_printf (_("usage: xmms2 <%s> [args]\n\n"), sing);
	g_printf (_("Available %s:\n"), plur);
	g_list_foreach (cmdnames_find (names, NULL),
	                help_short_command, NULL);
	g_printf (_("\nType 'help <%s>' for detailed help about %s %s.\n"),
	          sing, det, sing);
}

static void
help_list_subcommands (GList *names, gchar *cmd,
                       const gchar *sing, const gchar *plur, const gchar *det)
{
	gchar **cmdv = g_strsplit (cmd, " ", 0);
	g_printf (_("usage: xmms2 %s <sub%s> [args]\n\n"), cmd, sing);
	g_printf (_("Available '%s' sub%s:\n"), cmd, plur);
	g_list_foreach (cmdnames_find (names, cmdv),
	                help_short_command, NULL);
	g_printf (_("\nType 'help %s <sub%s>' for detailed help "
	            "about a sub%s.\n"), cmd, sing, sing);
	g_strfreev (cmdv);
}

static void
help_list (GList *names, gchar *cmd, cmd_type_t cmdtype)
{
	const gchar *cmdtxt_sing, *cmdtxt_plur, *cmdtxt_det;

	/* This is a bit tedious, english-- */
	switch (cmdtype) {
	case CMD_TYPE_ALIAS:
		cmdtxt_sing = _("alias");
		cmdtxt_plur = _("aliases");
		cmdtxt_det  = _("an");
		break;
	case CMD_TYPE_COMMAND:
	default:
		cmdtxt_sing = _("command");
		cmdtxt_plur = _("commands");
		cmdtxt_det  = _("a");
		break;
	}

	if (!cmd) {
		help_list_commands (names, cmdtxt_sing, cmdtxt_plur, cmdtxt_det);
	} else {
		help_list_subcommands (names, cmd, cmdtxt_sing, cmdtxt_plur, cmdtxt_det);
	}
}

static void
print_indented (const gchar *string, guint level)
{
	gboolean indent = TRUE;
	const gchar *c;

	for (c = string; *c; c++) {
		if (indent) {
			print_padding (level, ' ');
			indent = FALSE;
		}
		g_printf ("%c", *c);
		if (*c == '\n') {
			indent = TRUE;
		}
	}
}

void
help_command (cli_infos_t *infos, GList *cmdnames, gchar **cmd, gint num_args,
              cmd_type_t cmdtype)
{
	command_action_t *action;
	command_trie_match_type_t match;
	gint i, k;
	gint padding, max_flag_len = 0;

	match = cli_infos_find_command (infos, &cmd, &num_args, &action);
	if (match == COMMAND_TRIE_MATCH_ACTION) {
		g_printf (_("usage: %s"), action->name);
		if (action->usage) {
			g_printf (" %s", action->usage);
		}
		g_printf ("\n\n");
		print_indented (action->description, COMMAND_HELP_DESCRIPTION_INDENT);
		g_printf ("\n\n");
		if (action->argdefs && action->argdefs[0].long_name) {
			/* Find length of longest option */
			for (i = 0; action->argdefs[i].long_name; ++i) {
				if (max_flag_len < strlen (action->argdefs[i].long_name)) {
					max_flag_len = strlen (action->argdefs[i].long_name);
				}
			}

			g_printf (_("Valid options:\n"));
			for (i = 0; action->argdefs[i].long_name; ++i) {
				padding = max_flag_len - strlen (action->argdefs[i].long_name) + 2;

				if (action->argdefs[i].short_name) {
					g_printf ("  -%c, ", action->argdefs[i].short_name);
				} else {
					g_printf ("      ");
				}

				g_printf ("--%s", action->argdefs[i].long_name);

				for (k = 0; k < padding; ++k) {
					g_printf (" ");
				}
				g_printf ("%s\n", action->argdefs[i].description);
				/* FIXME: align multi-line */
			}
		}
	} else if (match == COMMAND_TRIE_MATCH_SUBTRIE) {
		help_list (cmdnames, action->name, cmdtype);
	} else {
		/* FIXME: Better handle help for subcommands! */
		g_printf (_("Unknown command: '"));
		for (i = 0; i < num_args; ++i) {
			if (i > 0) g_printf (" ");
			g_printf ("%s", cmd[i]);
		}
		g_printf (_("'\n"));
		g_printf (_("Type 'help' for the list of commands.\n"));
	}
}

gboolean
cli_help (cli_infos_t *infos, command_context_t *ctx)
{
	cmd_type_t cmdtype;
	GList *names;
	gint num_args;
	gboolean alias;

	num_args = command_arg_count (ctx);

	if (command_flag_boolean_get (ctx, "alias", &alias) && alias) {
		names = cli_infos_alias_names (infos);
		cmdtype = CMD_TYPE_ALIAS;
	} else {
		names = cli_infos_command_names (infos);
		cmdtype = CMD_TYPE_COMMAND;
	}

	/* No argument, display the list of commands */
	if (num_args == 0) {
		help_list (names, NULL, cmdtype);
	} else {
		help_command (infos, names, command_argv_get (ctx), num_args, cmdtype);
	}

	/* No data pending */
	return FALSE;
}
