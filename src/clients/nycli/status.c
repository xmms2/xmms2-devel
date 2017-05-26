/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "status.h"

struct status_entry_St {
	status_free_func_t free_func;
	status_refresh_func_t refresh_func;
	status_callback_func_t callback_func;
	gpointer udata;
	const keymap_entry_t *map;

	gint refresh;
};

status_entry_t *
status_init (status_free_func_t free_func, status_refresh_func_t refresh_func,
             status_callback_func_t callback_func,
             const keymap_entry_t map[], gpointer udata,
             gint refresh)
{
	status_entry_t *entry;

	entry = g_new0 (status_entry_t, 1);
	entry->free_func = free_func;
	entry->refresh_func = refresh_func;
	entry->callback_func = callback_func;
	entry->udata = udata;
	entry->map = map;
	entry->refresh = refresh;

	return entry;
}

void
status_free (status_entry_t *entry)
{
	entry->free_func (entry->udata);
	g_free (entry);
}

void
status_refresh (status_entry_t *entry, gboolean first, gboolean last)
{
	entry->refresh_func (entry->udata, first, last);
}

gint
status_get_refresh_interval (const status_entry_t *entry)
{
	return entry->refresh;
}

const keymap_entry_t *
status_get_keymap (const status_entry_t *entry) {
	return entry->map;
}

gint
status_call_callback (const status_entry_t *entry, gint i)
{
	if (entry->callback_func) {
		return entry->callback_func (i, entry->udata);
	} else {
		return 1;
	}
}
