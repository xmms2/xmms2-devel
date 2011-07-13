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

#include "xmmspriv/xmms_medialib.h"
#include "xmms/xmms_object.h"
#include <string.h>

struct xmms_medialib_session_St {
	xmms_medialib_t *medialib;
	s4_transaction_t *trans;
	GHashTable *added;
	GHashTable *updated;
	GHashTable *removed;
	xmmsv_t *vals;
};

static void xmms_medialib_session_free (xmms_medialib_session_t *session);
static void xmms_medialib_session_free_full (xmms_medialib_session_t *session);

static void xmms_medialib_entry_send_added (xmms_medialib_t *medialib, xmms_medialib_entry_t entry);
static void xmms_medialib_entry_send_update (xmms_medialib_t *medialib, xmms_medialib_entry_t entry);
static void xmms_medialib_entry_send_removed (xmms_medialib_t *medialib, xmms_medialib_entry_t entry);

xmms_medialib_session_t *
xmms_medialib_session_begin (xmms_medialib_t *medialib)
{
	xmms_medialib_session_t *ret = g_new0 (xmms_medialib_session_t, 1);

	xmms_object_ref (medialib);
	ret->medialib = medialib;

	s4_t *s4 = xmms_medialib_get_database_backend (medialib);
	ret->trans = s4_begin (s4, 0);

	ret->added = g_hash_table_new (NULL, NULL);
	ret->updated = g_hash_table_new (NULL, NULL);
	ret->removed = g_hash_table_new (NULL, NULL);

	ret->vals = xmmsv_new_list ();

	return ret;
}

void
xmms_medialib_session_abort (xmms_medialib_session_t *session)
{
	s4_abort (session->trans);
	xmms_medialib_session_free_full (session);
}

gboolean
xmms_medialib_session_commit (xmms_medialib_session_t *session)
{
	GHashTableIter iter;
	gpointer key;

	if (!s4_commit (session->trans)) {
		xmms_medialib_session_free_full (session);
		return FALSE;
	}

	g_hash_table_iter_init (&iter, session->added);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		xmms_medialib_entry_send_added (session->medialib,
		                                GPOINTER_TO_INT (key));
		g_hash_table_remove (session->updated, key);
	}

	g_hash_table_iter_init (&iter, session->removed);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		xmms_medialib_entry_send_removed (session->medialib,
		                                  GPOINTER_TO_INT (key));
		g_hash_table_remove (session->updated, key);
	}

	g_hash_table_iter_init (&iter, session->updated);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		xmms_medialib_entry_send_update (session->medialib,
		                                 GPOINTER_TO_INT (key));
	}

	xmms_medialib_session_free (session);

	return TRUE;
}

s4_sourcepref_t *
xmms_medialib_session_get_source_preferences (xmms_medialib_session_t *session)
{
	return xmms_medialib_get_source_preferences (session->medialib);
}

s4_resultset_t *
xmms_medialib_session_query (xmms_medialib_session_t *session,
                             s4_fetchspec_t *specification,
                             s4_condition_t *condition)
{
	return s4_query (session->trans, specification, condition);
}

gint
xmms_medialib_session_property_set (xmms_medialib_session_t *session,
                                    xmms_medialib_entry_t entry,
                                    const gchar *key,
                                    const s4_val_t *value,
                                    const gchar *source)
{
	const gchar *sources[2] = { source, NULL };
	const s4_result_t *res;
	s4_condition_t *cond;
	s4_fetchspec_t *spec;
	s4_resultset_t *set;
	s4_sourcepref_t *sp;
	s4_val_t *song_id;
	gint result;
	GHashTable *events;

	song_id = s4_val_new_int (entry);

	sp = s4_sourcepref_create (sources);

	cond = s4_cond_new_filter (S4_FILTER_EQUAL, "song_id", song_id,
	                           sp, S4_CMP_CASELESS, S4_COND_PARENT);

	spec = s4_fetchspec_create ();
	s4_fetchspec_add (spec, key, sp, S4_FETCH_DATA);

	set = s4_query (session->trans, spec, cond);
	s4_cond_free (cond);
	s4_fetchspec_free (spec);

	res = s4_resultset_get_result (set, 0, 0);
	if (res != NULL) {
		const s4_val_t *old_value = s4_result_get_val (res);
		s4_del (session->trans, "song_id", song_id,
		        key, old_value, source);
	}

	s4_resultset_free (set);
	s4_sourcepref_unref (sp);

	result = s4_add (session->trans, "song_id", song_id,
	                 key, value, source);

	s4_val_free (song_id);

	if (strcmp (key, XMMS_MEDIALIB_ENTRY_PROPERTY_URL) == 0) {
		events = session->added;
	} else {
		events = session->updated;
	}

	g_hash_table_insert (events,
	                     GINT_TO_POINTER (entry),
	                     GINT_TO_POINTER (entry));

	return result;
}

gint
xmms_medialib_session_property_unset (xmms_medialib_session_t *session,
                                      xmms_medialib_entry_t entry,
                                      const gchar *key,
                                      const s4_val_t *value,
                                      const gchar *source)
{
	GHashTable *events;
	s4_val_t *song_id;
	gint result;

	song_id = s4_val_new_int (entry);
	result = s4_del (session->trans, "song_id", song_id,
	                 key, value, source);
	s4_val_free (song_id);

	if (strcmp (key, XMMS_MEDIALIB_ENTRY_PROPERTY_URL) == 0) {
		events = session->removed;
	} else {
		events = session->updated;
	}

	g_hash_table_insert (events,
	                     GINT_TO_POINTER (entry),
	                     GINT_TO_POINTER (entry));

	return result;
}

void
xmms_medialib_session_track_garbage (xmms_medialib_session_t *session,
                                     xmmsv_t *data)
{
	xmmsv_list_append (session->vals, data);
}


static void
xmms_medialib_session_free_full (xmms_medialib_session_t *session)
{
	xmmsv_t *val;
	gint i;

	for (i = 0; xmmsv_list_get (session->vals, i, &val); i++)
		xmmsv_unref (val);

	xmms_medialib_session_free (session);
}

static void
xmms_medialib_session_free (xmms_medialib_session_t *session)
{
	xmms_object_unref (session->medialib);

	g_hash_table_unref (session->added);
	g_hash_table_unref (session->updated);
	g_hash_table_unref (session->removed);

	xmmsv_unref (session->vals);

	g_free (session);
}

/**
 * Trigger an added siginal to the client. This should be
 * called when a new entry has been added to the medialib
 *
 * @param entry Entry to signal an add for.
 */
static void
xmms_medialib_entry_send_added (xmms_medialib_t *medialib, xmms_medialib_entry_t entry)
{
	xmms_object_emit_f (XMMS_OBJECT (medialib),
	                    XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_ADDED,
	                    XMMSV_TYPE_INT32, entry);
}

/**
 * Trigger a update signal to the client. This should be called
 * when important information in the entry has been changed and
 * should be visible to the user.
 *
 * @param entry Entry to signal a update for.
 */

static void
xmms_medialib_entry_send_update (xmms_medialib_t *medialib, xmms_medialib_entry_t entry)
{
	xmms_object_emit_f (XMMS_OBJECT (medialib),
	                    XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE,
	                    XMMSV_TYPE_INT32, entry);
}

/**
 * Trigger a removed siginal to the client. This should be
 * called when an entry has been removed from the medialib
 *
 * @param entry Entry to signal a remove for.
 */
static void
xmms_medialib_entry_send_removed (xmms_medialib_t *medialib, xmms_medialib_entry_t entry)
{
	xmms_object_emit_f (XMMS_OBJECT (medialib),
	                    XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_REMOVED,
	                    XMMSV_TYPE_INT32, entry);
}

