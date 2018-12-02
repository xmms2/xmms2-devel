/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2018 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifdef HAVE_SC68_OLD
# include <api68/api68.h>
#else
# include <sc68/sc68.h>
#endif /* HAVE_SC68_OLD */

#ifdef HAVE_SC68_OLD
# define SC68_LOOP  (API68_LOOP)
# define SC68_END   (API68_END)
# define SC68_ERROR (API68_MIX_ERROR)

# define sc68_t            api68_t
# define sc68_init_t       api68_init_t
# define sc68_disk_t       api68_disk_t
# define sc68_music_info_t api68_music_info_t

# define sc68_init          api68_init
# define sc68_open          api68_open
# define sc68_close         api68_close
# define sc68_error         api68_error
# define sc68_disk_free     api68_free
# define sc68_disk_load_mem api68_disk_load_mem
# define sc68_music_info    api68_music_info
# define sc68_process       api68_process
# define sc68_play          api68_play

# define xmms_sc68_verify_mem(a,b)     ( api68_verify_mem(a,b) )
#else
# define xmms_sc68_verify_mem(a,b)     ( 0 )
#endif /* HAVE_SC68_OLD */

const char *xmms_sc68_error (sc68_t *sc68);
sc68_t *xmms_sc68_api_init(sc68_init_t *settings);
int xmms_sc68_process (sc68_t *sc68, void *buffer, int *n);
void xmms_sc68_extract_metadata (xmms_xform_t *xform,
                                 sc68_music_info_t *disk_info);
