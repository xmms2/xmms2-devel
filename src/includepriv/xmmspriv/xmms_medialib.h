/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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




#ifndef __XMMS_PRIV_MEDIALIB_H__
#define __XMMS_PRIV_MEDIALIB_H__

#include "xmms/xmms_medialib.h"

typedef struct xmms_medialib_St xmms_medialib_t;
typedef struct xmms_medialib_session_St xmms_medialib_session_t;

#include "xmmspriv/xmms_collection.h"
#include "xmmspriv/xmms_fetch_info.h"
#include "xmmspriv/xmms_fetch_spec.h"
#include <s4.h>

xmms_medialib_t *xmms_medialib_init (void);
s4_t *xmms_medialib_get_database_backend (xmms_medialib_t *medialib);
s4_sourcepref_t *xmms_medialib_get_source_preferences (xmms_medialib_t *medialib);
char *xmms_medialib_uuid (xmms_medialib_t *mlib);
s4_resultset_t *xmms_medialib_session_query (xmms_medialib_session_t *s, s4_fetchspec_t *spec, s4_condition_t *cond);

guint xmms_medialib_num_not_resolved (xmms_medialib_session_t *s);
xmms_medialib_entry_t xmms_medialib_entry_not_resolved_get (xmms_medialib_session_t *s);

xmms_medialib_entry_t xmms_medialib_entry_new (xmms_medialib_session_t *s, const char *url, xmms_error_t *error);
xmms_medialib_entry_t xmms_medialib_entry_new_encoded (xmms_medialib_session_t *s, const char *url, xmms_error_t *error);

void xmms_medialib_entry_remove (xmms_medialib_session_t *s, xmms_medialib_entry_t entry);
void xmms_medialib_entry_cleanup (xmms_medialib_session_t *s, xmms_medialib_entry_t entry);

gint xmms_medialib_entry_property_get_int (xmms_medialib_session_t *s, xmms_medialib_entry_t entry, const gchar *property);
gchar *xmms_medialib_entry_property_get_str (xmms_medialib_session_t *s, xmms_medialib_entry_t entry, const gchar *property);
xmmsv_t *xmms_medialib_entry_property_get_value (xmms_medialib_session_t *s, xmms_medialib_entry_t entry, const gchar *property);

gboolean xmms_medialib_entry_property_set_int (xmms_medialib_session_t *s, xmms_medialib_entry_t entry, const gchar *property, gint value);
gboolean xmms_medialib_entry_property_set_str (xmms_medialib_session_t *s, xmms_medialib_entry_t entry, const gchar *property, const gchar *value);
gboolean xmms_medialib_entry_property_set_int_source (xmms_medialib_session_t *s, xmms_medialib_entry_t entry, const gchar *property, gint value, const gchar *source);
gboolean xmms_medialib_entry_property_set_str_source (xmms_medialib_session_t *s, xmms_medialib_entry_t entry, const gchar *property, const gchar *value, const gchar *source);

gchar *xmms_medialib_url_encode (const gchar *path);
gboolean xmms_medialib_decode_url (char *url);

gboolean xmms_medialib_check_id (xmms_medialib_session_t *s, xmms_medialib_entry_t entry);

xmmsv_coll_t *xmms_medialib_add_recursive (xmms_medialib_t *medialib, const gchar *path, xmms_error_t *error);

xmms_medialib_entry_t xmms_medialib_query_random_id (xmms_medialib_session_t *s, xmmsv_coll_t *coll);

xmmsv_t *xmms_medialib_query (xmms_medialib_session_t *s, xmmsv_coll_t *coll, xmmsv_t *fetch, xmms_error_t *err);
s4_resultset_t *xmms_medialib_query_recurs (xmms_medialib_session_t *session, xmmsv_coll_t *coll, xmms_fetch_info_t *fetch);
xmmsv_t *xmms_medialib_query_to_xmmsv (s4_resultset_t *set, xmms_fetch_spec_t *spec);


xmms_medialib_session_t *xmms_medialib_session_begin (xmms_medialib_t *mlib);
xmms_medialib_session_t *xmms_medialib_session_begin_ro (xmms_medialib_t *medialib);
void xmms_medialib_session_abort (xmms_medialib_session_t *session);
gboolean xmms_medialib_session_commit (xmms_medialib_session_t *session);
s4_resultset_t *xmms_medialib_session_query (xmms_medialib_session_t *session, s4_fetchspec_t *specification, s4_condition_t *condition);
s4_sourcepref_t *xmms_medialib_session_get_source_preferences (xmms_medialib_session_t *session);
void xmms_medialib_session_track_garbage (xmms_medialib_session_t *session, xmmsv_t *data);
gint xmms_medialib_session_property_set (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *key, const s4_val_t *value, const gchar *source);
gint xmms_medialib_session_property_unset (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *key, const s4_val_t *value, const gchar *source);

#define xmms_medialib_entry_status_set(s, e, st) xmms_medialib_entry_property_set_int_source(s, e, XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS, st, "server") /** @todo: hardcoded server id might be bad? */


#endif
