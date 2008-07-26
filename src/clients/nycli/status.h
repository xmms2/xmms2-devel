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

#ifndef __STATUS_H__
#define __STATUS_H__

#include "main.h"

struct status_entry_St {
	GHashTable *data;
	GList *format;
	gint refresh;
};

status_entry_t *status_init (gchar *format, gint refresh);
void status_free (status_entry_t *entry);
void status_update_all (cli_infos_t *infos, status_entry_t *entry);
void status_print_entry (status_entry_t *entry);

#endif /* __STATUS_H__ */
