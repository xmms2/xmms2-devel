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

#include "main.h"

gboolean command_flag_int_get (command_context_t *ctx, const gchar *name, gint *v);
gboolean command_flag_string_get (command_context_t *ctx, const gchar *name, gchar **v);
gint command_arg_count (command_context_t *ctx);
gboolean command_arg_int_get (command_context_t *ctx, gint at, gint *v);
gboolean command_arg_string_get (command_context_t *ctx, gint at, gchar **v);
gboolean command_arg_longstring_get (command_context_t *ctx, gint at, gchar **v);

#endif /* __COMMAND_UTILS_H__ */
