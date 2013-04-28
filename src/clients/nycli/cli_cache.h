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

#ifndef __CLI_CACHE_H__
#define __CLI_CACHE_H__

typedef struct cli_cache_St cli_cache_t;
typedef struct freshness_St freshness_t;
typedef enum cli_cache_status_St cli_cache_status_t;

#include <glib.h>
#include <xmmsclient/xmmsclient.h>

#include "cli_infos.h"

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
	gint32 currpos;
	gint32 currid;
	gint32 playback_status;
	xmmsv_t *active_playlist;
	gchar *active_playlist_name;

	/* Freshness of each attribute */
	freshness_t freshness_currpos;
	freshness_t freshness_currid;
	freshness_t freshness_playback_status;
	freshness_t freshness_active_playlist;
	freshness_t freshness_active_playlist_name;
};

cli_cache_t *cli_cache_init (void);
void cli_cache_start (cli_infos_t *infos);
gboolean cli_cache_is_fresh (cli_cache_t *cache);
void cli_cache_refresh (cli_infos_t *infos);
void cli_cache_free (cli_cache_t *cache);

#endif /* __CLI_INFOS_H__ */
