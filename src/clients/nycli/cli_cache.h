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


struct cli_cache_St {
	guint currpos;
	guint active_playlist_size;
	GArray *active_playlist;
};

cli_cache_t *cli_cache_init ();
void cli_cache_start (cli_infos_t *infos);
void cli_cache_free (cli_cache_t *cache);

#endif /* __CLI_INFOS_H__ */
