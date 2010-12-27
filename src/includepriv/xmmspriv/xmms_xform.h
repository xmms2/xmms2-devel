/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#ifndef __XMMS_PRIV_XFORM_H__
#define __XMMS_PRIV_XFORM_H__

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_medialib.h"
#include "xmmspriv/xmms_streamtype.h"
#include "xmmspriv/xmms_plugin.h"

typedef struct xmms_xform_object_St xmms_xform_object_t;

xmms_xform_object_t *xmms_xform_object_init (void);

xmms_xform_t *xmms_xform_new (xmms_xform_plugin_t *plugin, xmms_xform_t *prev, xmms_medialib_entry_t entry, GList *goal_hints);
const gchar *xmms_xform_outtype_get_str (xmms_xform_t *xform, xmms_stream_type_key_t key);
gint xmms_xform_outtype_get_int (xmms_xform_t *xform, xmms_stream_type_key_t key);
xmms_stream_type_t *xmms_xform_outtype_get (xmms_xform_t *xform);

xmms_plugin_t *xmms_xform_plugin_new (void);
gboolean xmms_xform_plugin_verify (xmms_plugin_t *plugin);

xmms_xform_t *xmms_xform_chain_setup (xmms_medialib_entry_t entry, GList *goal_formats, gboolean rehash);
xmms_xform_t *xmms_xform_chain_setup_session (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, GList *goal_fmts, gboolean rehash);
xmms_xform_t *xmms_xform_chain_setup_url_session (xmms_medialib_session_t *session, xmms_medialib_entry_t entry, const gchar *url, GList *goal_fmts, gboolean rehash);
xmms_xform_t *xmms_xform_chain_setup_url (xmms_medialib_entry_t entry, const gchar *url, GList *goal_formats, gboolean rehash);

gint64 xmms_xform_this_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *err);
int xmms_xform_this_read (xmms_xform_t *xform, gpointer buf, int siz, xmms_error_t *err);
gboolean xmms_xform_iseos (xmms_xform_t *xform);

const GList *xmms_xform_goal_hints_get (xmms_xform_t *xform);
xmms_stream_type_t *xmms_xform_intype_get (xmms_xform_t *xform);

void xmms_xform_outdata_type_set (xmms_xform_t *xform, xmms_stream_type_t *type);
GList *xmms_xform_browse (const gchar *url, xmms_error_t *error);
GList *xmms_xform_browse_method (xmms_xform_t *xform, const gchar *url, xmms_error_t *error);

const char *xmms_xform_indata_find_str (xmms_xform_t *xform, xmms_stream_type_key_t key);

#define XMMS_XFORM_BUILTIN(shname, name, ver, desc, setupfunc) XMMS_BUILTIN(XMMS_PLUGIN_TYPE_XFORM, XMMS_XFORM_API_VERSION, shname, name, ver, desc, (gboolean (*)(gpointer))setupfunc)

#endif
