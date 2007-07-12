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

#ifndef __CLI_CACHE_H__
#define __CLI_CACHE_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <stdlib.h>

#include "main.h"

typedef struct freshness_St freshness_t;
typedef enum cli_cache_status_St cli_cache_status_t;

enum cli_cache_status_St {
	CLI_CACHE_NOT_INIT,
	CLI_CACHE_PENDING,
	CLI_CACHE_FRESH
};

struct freshness_St {
	cli_cache_status_t status;
	gint pending_queries;
};

struct cli_cache_St {
	guint currpos;
	GArray *active_playlist;
	gchar *active_playlist_name;

	/* Freshness of each attribute */
	freshness_t freshness_currpos;
	freshness_t freshness_active_playlist;
	freshness_t freshness_active_playlist_name;
};

cli_cache_t *cli_cache_init ();
void cli_cache_start (cli_infos_t *infos);
gboolean cli_cache_is_fresh (cli_cache_t *cache);
void cli_cache_free (cli_cache_t *cache);

#endif /* __CLI_INFOS_H__ */
