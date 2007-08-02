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

#include "udata_packs.h"


struct pack_infos_playlist_St {
	cli_infos_t *infos;
	gchar *playlist;     /* owned by this struct */
};


pack_infos_playlist_t *
pack_infos_playlist (cli_infos_t *infos, gchar *playlist)
{
	pack_infos_playlist_t *pack = g_new0 (pack_infos_playlist_t, 1);
	pack->infos = infos;
	pack->playlist = g_strdup (playlist);
	return pack;
}

void
unpack_infos_playlist (pack_infos_playlist_t *pack, cli_infos_t **infos,
                       gchar **playlist)
{
	*infos = pack->infos;
	*playlist = pack->playlist;
}

void
free_infos_playlist (pack_infos_playlist_t *pack)
{
	g_free (pack->playlist);
	g_free (pack);
}
