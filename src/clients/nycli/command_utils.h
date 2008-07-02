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

#ifndef __COMMAND_UTILS_H__
#define __COMMAND_UTILS_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

#define MAX_STRINGLIST_TOKENS 10

typedef struct command_arg_time_St command_arg_time_t;

typedef enum {
	COMMAND_ARG_TIME_POSITION,
	COMMAND_ARG_TIME_OFFSET
} command_arg_time_type_t;

struct command_arg_time_St {
	union {
		guint pos;
		gint offset;
	} value;
	command_arg_time_type_t type;
};

gboolean command_flag_boolean_get (command_context_t *ctx, const gchar *name, gboolean *v);
gboolean command_flag_int_get (command_context_t *ctx, const gchar *name, gint *v);
gboolean command_flag_string_get (command_context_t *ctx, const gchar *name, gchar **v);
gboolean command_flag_stringlist_get (command_context_t *ctx, const gchar *name, const gchar ***v);
gint command_arg_count (command_context_t *ctx);
gchar** command_argv_get (command_context_t *ctx);
gboolean command_arg_int_get (command_context_t *ctx, gint at, gint *v);
gboolean command_arg_string_get (command_context_t *ctx, gint at, gchar **v);
gboolean command_arg_longstring_get (command_context_t *ctx, gint at, gchar **v);
gboolean command_arg_time_get (command_context_t *ctx, gint at, command_arg_time_t *v);
gboolean command_arg_pattern_get (command_context_t *ctx, gint at, xmmsc_coll_t **v, gboolean warn);


typedef void (*double_notifier_f)(xmmsc_result_t *res1, xmmsc_result_t *res2, void *user_data);
typedef struct double_callback_infos_St double_callback_infos_t;

void register_double_callback (xmmsc_result_t *res1, xmmsc_result_t *res2, double_notifier_f cb, void *udata);


#endif /* __COMMAND_UTILS_H__ */
