/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#ifndef __IPC_SIGNAL_EXTRA_XMMS_H__

#ifndef __IPC_SIGNAL_XMMS_H__
# error "This header can't be included directly. Include xmmsc/xmmsc_idnumbers.h instead."
#endif

#define __IPC_SIGNAL_EXTRA_XMMS_H__

/* Alias to generated names to preserve compatibility */
typedef xmms_collection_type_t xmmsv_coll_type_t;
typedef xmms_log_level_t xmmsc_log_level_t;
typedef xmms_medialib_entry_status_t xmmsc_medialib_entry_status_t;

typedef const char* xmmsv_coll_namespace_t;
#define	XMMS_COLLECTION_NS_ALL          "*"
#define XMMS_COLLECTION_NS_COLLECTIONS  "Collections"
#define XMMS_COLLECTION_NS_PLAYLISTS    "Playlists"

#define XMMS_ACTIVE_PLAYLIST "_active"

/* Default source preferences for accessing "propdicts" (decl. in value.c) */
extern const char *xmmsv_default_source_pref[] XMMS_PUBLIC;

/* compability */
typedef xmmsv_coll_type_t xmmsc_coll_type_t;
typedef xmmsv_coll_namespace_t xmmsc_coll_namespace_t;

#endif /* __IPC_SIGNAL_EXTRA_XMMS_H__ */
