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

#include "commands.h"

#include "cli_infos.h"


static gboolean command_flag_int_get (command_context_t *ctx, const gchar *name, gint *v);
static gboolean command_flag_string_get (command_context_t *ctx, const gchar *name, gchar **v);


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
	/* FIXME: Actually we need a connection. We just don't want to
	 * start it for nothing. */
	xmmsc_quit (infos->conn);
	return TRUE;
}

gboolean cli_exit (cli_infos_t *infos, command_context_t *ctx)
{
	cli_infos_loop_stop (infos);
	return TRUE;
}
