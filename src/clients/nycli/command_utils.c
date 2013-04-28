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
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "command_utils.h"
#include "main.h"


#define command_arg_get(ctx, at) (ctx)->argv[(at) + 1]

static void command_argument_free (void *x);


typedef union {
	gboolean vbool;
	gint vint;
	gchar *vstring;
	gchar **vstringv;
} command_argument_value_t;

typedef enum {
	COMMAND_ARGUMENT_TYPE_BOOLEAN = G_OPTION_ARG_NONE,
	COMMAND_ARGUMENT_TYPE_INT = G_OPTION_ARG_INT,
	COMMAND_ARGUMENT_TYPE_STRING = G_OPTION_ARG_STRING,
	COMMAND_ARGUMENT_TYPE_STRING_ARRAY = G_OPTION_ARG_STRING_ARRAY,
} command_argument_type_t;

typedef struct {
	command_argument_type_t type;
	command_argument_value_t value;
} command_argument_t;

struct command_context_St {
	gchar *name;
	gint argc;
	gchar **argv;
	GHashTable *flags;
};

/* Parse the argv array with GOptionContext, using the given argument
 * definitions, and return a command context structure containing
 * argument values.
 * Note: The lib doesn't like argv starting with a flag, so keep a
 * token before that to avoid problems.
 *
 * The passed argv should be an array of length argc+1. (So that a terminating
 * NULL-pointer can be added in argv[argc].)
 */
command_context_t *
command_context_new (GOptionEntry *argdefs, gint argc, gchar **argv)
{
	command_context_t *ctx;
	GOptionContext *context;
	GError *error = NULL;
	gint i;

	ctx = g_new0 (command_context_t, 1);
	ctx->argc = argc;
	ctx->argv = argv;
	ctx->flags = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                    g_free, command_argument_free);

	for (i = 0; argdefs && argdefs[i].long_name; ++i) {
		command_argument_t *arg = g_new (command_argument_t, 1);

		switch (argdefs[i].arg) {
			case G_OPTION_ARG_NONE:
				arg->type = COMMAND_ARGUMENT_TYPE_BOOLEAN;
				arg->value.vbool = FALSE;
				argdefs[i].arg_data = &arg->value.vbool;
				break;

			case G_OPTION_ARG_INT:
				arg->type = COMMAND_ARGUMENT_TYPE_INT;
				arg->value.vint = -1;
				argdefs[i].arg_data = &arg->value.vint;
				break;

			case G_OPTION_ARG_STRING:
				arg->type = COMMAND_ARGUMENT_TYPE_STRING;
				arg->value.vstring = NULL;
				argdefs[i].arg_data = &arg->value.vstring;
				break;

			case G_OPTION_ARG_STRING_ARRAY:
				arg->type = COMMAND_ARGUMENT_TYPE_STRING_ARRAY;
				arg->value.vstringv = NULL;
				argdefs[i].arg_data = &arg->value.vstringv;
				break;

			default:
				g_printf (_("Trying to register a flag '%s' of invalid type!"),
				          argdefs[i].long_name);
				break;
		}

		g_hash_table_insert (ctx->flags,
		                     g_strdup (argdefs[i].long_name), arg);
	}

	context = g_option_context_new (NULL);
	g_option_context_set_help_enabled (context, FALSE);  /* runs exit(0)! */
	g_option_context_set_ignore_unknown_options (context, TRUE);
	g_option_context_add_main_entries (context, argdefs, NULL);
	g_option_context_parse (context, &ctx->argc, &ctx->argv, &error);
	g_option_context_free (context);

	if (error) {
		g_printf (_("Error: %s\n"), error->message);
		g_error_free (error);
		command_context_free (ctx);
		return NULL;
	}

	/* strip --, check for unknown options before it */
	/* FIXME: We do not parse options elsewhere, do we? */
	for (i = 0; i < ctx->argc; i++) {
		if (strcmp (ctx->argv[i], "--") == 0) {
			break;
		}
		if (ctx->argv[i][0] == '-' && ctx->argv[i][1] != '\0' &&
		    !(ctx->argv[i][1] >= '0' && ctx->argv[i][1] <= '9')) {

			g_printf (_("Error: Unknown option '%s'\n"), ctx->argv[i]);
			command_context_free (ctx);
			return NULL;
		}
	}
	if (i != ctx->argc) {
		for (i++; i < ctx->argc; i++) {
			argv[i-1] = argv[i];
		}
		ctx->argc--;
	}

	/* Some functions rely on NULL-termination. */
	ctx->argv[ctx->argc] = NULL;
	return ctx;
}


static void
command_argument_free (void *x)
{
	command_argument_t *arg = (command_argument_t *)x;

	if (arg->type == COMMAND_ARGUMENT_TYPE_STRING && arg->value.vstring) {
		g_free (arg->value.vstring);
	}
	g_free (arg);
}

void
command_context_free (command_context_t *ctx)
{
	g_hash_table_destroy (ctx->flags);
	g_free (ctx->name);
	g_free (ctx);
}

gboolean
command_flag_boolean_get (command_context_t *ctx, const gchar *name, gboolean *v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_BOOLEAN) {
		*v = arg->value.vbool;
		retval = TRUE;
	}

	return retval;
}

gboolean
command_flag_int_get (command_context_t *ctx, const gchar *name, gint *v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	/* A negative value means the value was not set. */
	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_INT && arg->value.vint >= 0) {
		*v = arg->value.vint;
		retval = TRUE;
	}

	return retval;
}

gboolean
command_flag_string_get (command_context_t *ctx, const gchar *name, const gchar **v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_STRING && arg->value.vstring) {
		*v = arg->value.vstring;
		retval = TRUE;
	}

	return retval;
}

/* Extract the flag value as a list of string items.
 * Warning: the resulting string must be freed using g_strfreev() !*/
gboolean
command_flag_stringlist_get (command_context_t *ctx, const gchar *name, const gchar ***v)
{
	const gchar *full;
	gboolean retval = FALSE;

	if (command_flag_string_get (ctx, name, &full) && full) {
		/* Force cast to suppress warning, Don't panic! */
		*v = (const gchar **) g_strsplit (full, ",", MAX_STRINGLIST_TOKENS);
		retval = TRUE;
	}

	return retval;
}

gboolean
command_flag_stringarray_get (command_context_t *ctx, const gchar *name, const gchar ***v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_STRING_ARRAY && arg->value.vstringv) {
		*v = (const gchar **) arg->value.vstringv;
		retval = TRUE;
	}

	return retval;
}

void
command_name_set (command_context_t *ctx, const gchar *name)
{
	ctx->name = g_strdup (name);
}

gchar *
command_name_get (command_context_t *ctx)
{
	return ctx->name;
}

gint
command_arg_count (command_context_t *ctx)
{
	return ctx->argc - 1;
}

gchar **
command_argv_get (command_context_t *ctx)
{
	return ctx->argv + 1;
}

gboolean
command_arg_int_get (command_context_t *ctx, gint at, gint *v)
{
	gboolean retval = FALSE;

	if (at < command_arg_count (ctx)) {
		*v = strtol (command_arg_get (ctx, at), NULL, 10);
		retval = TRUE;
	}

	return retval;
}

gboolean
command_arg_string_get (command_context_t *ctx, gint at, const gchar **v)
{
	gboolean retval = FALSE;

	if (at < command_arg_count (ctx)) {
		*v = command_arg_get (ctx, at);
		retval = TRUE;
	}

	return retval;
}

/* Grab all the arguments after the index as a single string.
 * Warning: the string must be freed manually afterwards!
 */
gboolean
command_arg_longstring_get (command_context_t *ctx, gint at, gchar **v)
{
	gboolean retval = FALSE;

	if (at < command_arg_count (ctx)) {
		*v = g_strjoinv (" ", &(command_arg_get (ctx, at)));
		retval = TRUE;
	}

	return retval;
}

/* Escape characters in toescape with escape_char.
 */
static gchar *
strescape (gchar *s, const gchar *toescape, gchar escape_char)
{
	gint len;
	gchar *t, *r;

	t = s;
	for (len = 0; *t != '\0'; len++, t++) {
		if (strchr (toescape, *t)) {
			len++;
		}
	}
	r = g_new0 (gchar, len+1);
	t = r;
	while (*s) {
		if (strchr (toescape, *s)) {
			*t = escape_char;
			t++;
		}
		*t = *s;
		s++;
		t++;
	}

	return r;
}

/* Like command_arg_longstring_get but escape spaces with '\'.
 */
gboolean
command_arg_longstring_get_escaped (command_context_t *ctx, gint at, gchar **v)
{
	gboolean retval = FALSE;
	gchar **args;
	gint i, len, count = command_arg_count (ctx);

	len = count-at+1;
	if (at < count) {
		args = g_new0 (gchar *, len);
		args[len-1] = NULL;
		for (i = at; i < count; i++) {
			args[i-at] = strescape (command_arg_get (ctx, i), " ", '\\');
		}
		*v = g_strjoinv (" ", args);

		for (i = at; i < count; i++) {
			g_free (args[i-at]);
		}
		g_free (args);

		retval = TRUE;
	}

	return retval;
}

static guint
parse_time_sep (const gchar *s, gchar **endptr)
{
	gint i;
	guint v;
	const gchar *t;

	v = 0;
	t = s;
	for (i = 0; i < 3 && *t != '\0'; i++) {
		v = v*60 + strtol (t, endptr, 10);
		t = *endptr+1;
	}

	return v;
}

/*
 * Parse time expressions of the form:
 *
 * e0: [0-9]*:[0-9]*:[0-9]
 * e1: ([0-9]*(hour|h|min|m|sec|s))*
 *
 * RFC: Be more restrictive?
 *
 *    Actually it accepts expressions like:
 *         1hour2min1hour1s for 2hour2min1s
 *         1min2hour7sec for 2hour1min7sec
 *
 */
static guint
parse_time (gchar *s, gchar **endptr, const gint *mul, const gchar **sep)
{
	gint i;
	guint n, v;

	if (strchr (s, ':') != NULL) {
		return parse_time_sep (s, endptr);
	}

	n = 0;
	v = 0;
	while (*s) {
		if ('0' <= *s && *s <= '9') {
			n = n*10 + (*s - '0');
			s++;
		} else {
			for (i = 0; sep[i] != NULL; i++) {
				if (g_str_has_prefix (s, sep[i])) {
					v += mul[i]*n;
					s += strlen (sep[i]);
					n = 0;
					break;
				}
			}
			if (sep[i] == NULL) {
				break;
			}
		}
	}
	v += n;
	*endptr = s;

	return v;
}

/* Parse a time value, either an absolute position or an offset. */
gboolean
command_arg_time_get (command_context_t *ctx, gint at, command_arg_time_t *v)
{
	gboolean retval = FALSE;
	const gchar *s;
	gchar *endptr;

	const gchar *separators[] = {"hour", "h", "min", "m", "sec", "s", NULL};
	const gint multipliers[] = {3600, 3600, 60, 60, 1, 1};

	if (at < command_arg_count (ctx) && command_arg_string_get (ctx, at, &s)) {
		gchar *time_arg = g_strdup (s);
		if (*time_arg == '+' || *time_arg == '-') {
			v->type = COMMAND_ARG_TIME_OFFSET;
			/* v->value.offset = strtol (s, &endptr, 10); */
			v->value.offset = parse_time (time_arg + 1, &endptr, multipliers, separators);
			if (*time_arg == '-') {
				v->value.offset = -v->value.offset;
			}
		} else {
			/* FIXME: always signed long int anyway? */
			v->type = COMMAND_ARG_TIME_POSITION;
			/* v->value.pos = strtol (s, &endptr, 10); */
			v->value.pos = parse_time (time_arg, &endptr, multipliers, separators);
		}

		/* FIXME: We can have cleverer parsing for '2:17' or '3min' etc */
		if (*endptr == '\0') {
			retval = TRUE;
		}

		g_free (time_arg);
	}

	return retval;
}

/** Try to parse a collection pattern from the arguments and return
 *  success status. The resulting coll structure is saved to the v
 *  argument. In case of error, the error message is printed on
 *  stdout.
 */
gboolean
command_arg_pattern_get (command_context_t *ctx, gint at, xmmsc_coll_t **v,
                         gboolean warn)
{
	gchar *pattern = NULL;
	gboolean success = TRUE;

	/* FIXME: Need a more elegant error system. */
	/* FIXME(g): Escape tokens? command_arg_longstring_get_escaped ? */

	command_arg_longstring_get_escaped (ctx, at, &pattern);
	if (!pattern) {
		if (warn) g_printf (_("Error: you must provide a pattern!\n"));
		success = FALSE;
	} else if (!xmmsc_coll_parse (pattern, v)) {
		if (warn) g_printf (_("Error: failed to parse the pattern!\n"));
		success = FALSE;
	}

	g_free (pattern);

	return success;
}

/** Try to parse a positions expression from the arguments and return
 *  success status. The resulting positions structure is saved to the p
 *  argument.
 */
gboolean
command_arg_positions_get (command_context_t *ctx, gint at,
                           playlist_positions_t **p, gint currpos)
{
	gchar *expression = NULL;
	gboolean success = TRUE;

	command_arg_longstring_get_escaped (ctx, at, &expression);
	if (!expression || !playlist_positions_parse (expression, p, currpos)) {
		success = FALSE;
	}

	g_free (expression);

	return success;
}
