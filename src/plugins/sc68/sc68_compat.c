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

#include <stdlib.h>
#include <glib.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_xformplugin.h>

#include "sc68.h"

#ifdef HAVE_SC68_OLD
 typedef void *(*sc68_alloc_t) (unsigned);
#endif /* HAVE_SC68_OLD */

const char *
xmms_sc68_error (sc68_t *sc68)
{
	const char *error, *log_error;

#ifdef HAVE_SC68_ERROR_NO_ARG
	error = sc68_error ();
#else
	error = sc68_error (sc68);
#endif /* HAVE_SC68_ERROR_NO_ARG */

	if (error) {
		xmms_log_info ("%s", error);
	}

#ifdef HAVE_SC68_ERROR_NO_ARG
	while ((log_error = sc68_error())) {
#else
	while ((log_error = sc68_error(sc68))) {
#endif /* HAVE_SC68_ERROR_NO_ARG */
		xmms_log_info ("%s", log_error);
	}

	return error;
}

sc68_t *
xmms_sc68_api_init(sc68_init_t *settings) {
#ifdef HAVE_SC68_OLD
	settings->alloc = (sc68_alloc_t) g_malloc;
	settings->free = g_free;

	return sc68_init (settings);
#else
	if(sc68_init(settings)) {
		return NULL;
	}
	return sc68_create(0);
#endif /* HAVE_SC68_OLD */
}

int
xmms_sc68_process (sc68_t *sc68, void *buffer, int *n) {
#ifdef HAVE_SC68_OLD
	return sc68_process (sc68, buffer, *n);
#else
	return sc68_process (sc68, buffer, n);
#endif /* HAVE_SC68_OLD */
}

void
xmms_sc68_extract_metadata (xmms_xform_t *xform,
                            sc68_music_info_t *disk_info) {
#ifdef HAVE_SC68_OLD
	xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,
	                             disk_info->title);
	xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,
	                             disk_info->author);
	xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_COMPOSER,
	                             disk_info->composer);
	xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_SUBTUNES,
	                             disk_info->tracks);
	xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
	                             disk_info->time_ms);
#else
	xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,
	                             disk_info->title);
	xmms_xform_metadata_set_str (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,
	                             disk_info->artist);
	xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_SUBTUNES,
	                             disk_info->tracks);
	xmms_xform_metadata_set_int (xform, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
	                             disk_info->dsk.time_ms);
#endif /* HAVE_SC68_OLD */
}
