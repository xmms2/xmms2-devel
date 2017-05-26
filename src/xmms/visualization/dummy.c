/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include "common.h"

int32_t
init_shm (xmms_visualization_t *vis, int32_t id, int32_t shmid, xmms_error_t *err)
{
	xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE,
	                "Shared Memory not supported by this platform!");
	return -1;
}

void cleanup_shm (xmmsc_vis_unixshm_t *t) {}

gboolean
write_start_shm (int32_t id, xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t **dest)
{ return FALSE; }

void write_finish_shm (int32_t id, xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t *dest) {}

gboolean
write_shm (xmmsc_vis_unixshm_t *t, xmms_vis_client_t *c, int32_t id, struct timeval *time, int channels, int size, short *buf)
{
	return FALSE;
}
