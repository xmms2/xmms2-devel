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


/** @file
 *  Manages the synchronization of collections to the database at 10 seconds
 *  after the last collections-change.
 */

#include <xmmspriv/xmms_collsync.h>
#include <xmmspriv/xmms_utils.h>

#include <xmms/xmms_config.h>
#include <xmms/xmms_ipc.h>
#include <xmms/xmms_log.h>

#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>


#define XMMS_COLL_SYNC_DELAY 10 * G_TIME_SPAN_SECOND

static void xmms_coll_sync_schedule_sync (xmms_object_t *object, xmmsv_t *val, gpointer udata);
static gpointer xmms_coll_sync_loop (gpointer udata);
static void xmms_coll_sync_destroy (xmms_object_t *object);

static void xmms_coll_sync_start (xmms_coll_sync_t *sync);
static void xmms_coll_sync_stop (xmms_coll_sync_t *sync);

static void xmms_coll_sync_client_sync (xmms_coll_sync_t *sync, xmms_error_t *err);

typedef enum xmms_coll_sync_state_t {
	XMMS_COLL_SYNC_STATE_IDLE,
	XMMS_COLL_SYNC_STATE_DELAYED,
	XMMS_COLL_SYNC_STATE_IMMEDIATE,
	XMMS_COLL_SYNC_STATE_SHUTDOWN
} xmms_coll_sync_state_t;

struct xmms_coll_sync_St {
	xmms_object_t object;

	xmms_config_property_t *config;

	xmms_coll_dag_t *dag;
	xmms_playlist_t *playlist;

	gchar *uuid;

	GThread *thread;
	GMutex mutex;
	GCond cond;

	xmms_coll_sync_state_t state;
};

#include "collsync_ipc.c"

/**
 * Get the collection-to-database-synchronization thread running.
 */
xmms_coll_sync_t *
xmms_coll_sync_init (const gchar *uuid, xmms_coll_dag_t *dag, xmms_playlist_t *playlist)
{
	xmms_coll_sync_t *sync;
	gchar *path;

	sync = xmms_object_new (xmms_coll_sync_t, xmms_coll_sync_destroy);

	sync->uuid = g_strdup (uuid);

	g_cond_init (&sync->cond);
	g_mutex_init (&sync->mutex);

	xmms_object_ref (dag);
	sync->dag = dag;

	xmms_object_ref (playlist);
	sync->playlist = playlist;

	path = XMMS_BUILD_PATH ("collections", "${uuid}.db");
	sync->config = xmms_config_property_register ("collection.directory", path,
	                                              xmms_coll_sync_schedule_sync, sync);
	g_free (path);

	/* Connection coll_sync_cb to some signals */
	xmms_object_connect (XMMS_OBJECT (dag),
	                     XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                     xmms_coll_sync_schedule_sync, sync);

	/* FIXME: These signals should trigger COLLECTION_CHANGED */
	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	                     xmms_coll_sync_schedule_sync, sync);

	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                     xmms_coll_sync_schedule_sync, sync);

	xmms_object_connect (XMMS_OBJECT (playlist),
	                     XMMS_IPC_SIGNAL_PLAYLIST_LOADED,
	                     xmms_coll_sync_schedule_sync, sync);

	xmms_coll_sync_register_ipc_commands (XMMS_OBJECT (sync));

	xmms_coll_sync_start (sync);

	return sync;
}

/**
 * Shutdown the collection-to-database-synchronization thread.
 */
static void
xmms_coll_sync_destroy (xmms_object_t *object)
{
	xmms_coll_sync_t *sync = (xmms_coll_sync_t *) object;

	g_return_if_fail (sync);

	XMMS_DBG ("Deactivating collection synchronizer object.");

	xmms_coll_sync_unregister_ipc_commands ();

	xmms_config_property_callback_remove (sync->config,
	                                      xmms_coll_sync_schedule_sync,
	                                      sync);

	xmms_object_disconnect (XMMS_OBJECT (sync->playlist),
	                        XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	                        xmms_coll_sync_schedule_sync, sync);

	xmms_object_disconnect (XMMS_OBJECT (sync->playlist),
	                        XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	                        xmms_coll_sync_schedule_sync, sync);

	xmms_object_disconnect (XMMS_OBJECT (sync->playlist),
	                        XMMS_IPC_SIGNAL_PLAYLIST_LOADED,
	                        xmms_coll_sync_schedule_sync, sync);

	xmms_object_disconnect (XMMS_OBJECT (sync->dag),
	                        XMMS_IPC_SIGNAL_COLLECTION_CHANGED,
	                        xmms_coll_sync_schedule_sync, sync);

	xmms_coll_sync_stop (sync);

	xmms_object_unref (sync->playlist);
	xmms_object_unref (sync->dag);

	g_mutex_clear (&sync->mutex);
	g_cond_clear (&sync->cond);
	g_free (sync->uuid);
}

static gboolean
xmms_coll_sync_prepare_path (const gchar *path, GError **error)
{
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
		return TRUE;

	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		/* TODO: This branch can be removed after release as it will only
		 * be used for installations that were running development
		 * builds between 0.8 and 0.9
		 */
		gint exit_status;
		gchar *cmdline;

		exit_status = 0;

		cmdline = g_strdup_printf ("_xmms2-migrate-collections-v0 %s", path);

		if (!g_spawn_command_line_sync (cmdline, NULL, NULL, &exit_status, NULL) || exit_status) {
			xmms_log_fatal ("Could not run \"%s\", try to run it manually", cmdline);
		}

		/* If migration for some reason failed, just move the failing
		 * directory and leave room for a fresh one.
		 */
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			gchar *legacy;
			gint64 now;
			gint res;

			now = g_get_real_time ();

			legacy = g_strdup_printf ("%s.%" G_GINT64_FORMAT ".legacy", path, now);
			res = g_rename (path, legacy);
			g_free (legacy);

			if (res != 0) {
				g_set_error (error, G_FILE_ERROR,
				             g_file_error_from_errno (errno),
				             "%s", g_strerror (errno));
				return FALSE;
			}
		}
	} else {
		gchar *dirname;
		gint res;

		dirname = g_path_get_dirname (path);
		res = g_mkdir_with_parents (dirname, 0755);
		g_free (dirname);

		if (res != 0) {
			g_set_error (error, G_FILE_ERROR,
			             g_file_error_from_errno (errno),
			             "%s", g_strerror (errno));
			return FALSE;
		}
	}

	return TRUE;
}

static gchar *
xmms_coll_sync_get_path (xmms_coll_sync_t *sync)
{
	const gchar *original;
	GString *result;
	gchar **parts;
	gint i;

	result = g_string_new ("");

	original = xmms_config_property_get_string (sync->config);

	parts = g_strsplit (original, "${uuid}", 0);

	for (i = 0; parts[i] != NULL; i++) {
		if (*parts[i] == '\0') {
			g_string_append (result, sync->uuid);
		} else {
			g_string_append (result, parts[i]);
		}
	}

	g_strfreev (parts);

	return g_string_free (result, FALSE);
}

static void
xmms_coll_sync_set_state (xmms_coll_sync_t *sync, xmms_coll_sync_state_t state)
{
	g_mutex_lock (&sync->mutex);

	if (sync->state != state) {
		sync->state = state;
		g_cond_signal (&sync->cond);
	}

	g_mutex_unlock (&sync->mutex);
}

static void
xmms_coll_sync_start (xmms_coll_sync_t *sync)
{
	g_return_if_fail (sync);
	g_return_if_fail (sync->thread == NULL);

	sync->state = XMMS_COLL_SYNC_STATE_IDLE;
	sync->thread = g_thread_new (	"x2 coll sync", xmms_coll_sync_loop, sync);
}


/**
 * Signal the sync loop to exit and join the sync thread.
 */
static void
xmms_coll_sync_stop (xmms_coll_sync_t *sync)
{
	g_return_if_fail (sync);
	g_return_if_fail (sync->thread != NULL);

	xmms_coll_sync_set_state (sync, XMMS_COLL_SYNC_STATE_SHUTDOWN);

	g_thread_join (sync->thread);

	sync->thread = NULL;
}

/**
 * Schedule a collection-to-database-synchronization in 10 seconds.
 */
static void
xmms_coll_sync_schedule_sync (xmms_object_t *object, xmmsv_t *val,
                              gpointer udata)
{
	xmms_coll_sync_t *sync = (xmms_coll_sync_t *) udata;

	g_return_if_fail (sync);

	xmms_coll_sync_set_state (sync, XMMS_COLL_SYNC_STATE_DELAYED);
}

/**
 * Schedule a collection-to-database-synchronization right away.
 */
static void
xmms_coll_sync_client_sync (xmms_coll_sync_t *sync, xmms_error_t *err)
{
	g_return_if_fail (sync);

	xmms_coll_sync_set_state (sync, XMMS_COLL_SYNC_STATE_IMMEDIATE);
}

static void
xmms_coll_sync_save (xmms_coll_sync_t *sync)
{
	GError *error = NULL;

	gchar *path = xmms_coll_sync_get_path (sync);

	XMMS_DBG ("Syncing collections to '%s'.", path);

	if (xmms_coll_sync_prepare_path (path, &error)) {
		xmmsv_t *snapshot, *serialized;
		const guchar *buffer;
		guint length;

		snapshot = xmms_collection_snapshot (sync->dag);

		serialized = xmmsv_serialize (snapshot);
		xmmsv_unref (snapshot);

		xmmsv_get_bin (serialized, &buffer, &length);

		if (!g_file_set_contents (path, (const gchar *) buffer, (gssize) length, &error)) {
			xmms_log_error ("Could not save collections to disk.");
		}

		xmmsv_unref (serialized);
	}

	if (error != NULL) {
		XMMS_DBG ("%s", error->message);
		g_error_free (error);
	}

	g_free (path);
}

static void
xmms_coll_sync_restore (xmms_coll_sync_t *sync)
{
	xmmsv_t *snapshot = NULL;
	GError *error = NULL;
	gchar *buffer;
	gsize length;

	gchar *path = xmms_coll_sync_get_path (sync);

	XMMS_DBG ("Restoring collections from '%s'.", path);

	if (xmms_coll_sync_prepare_path (path, &error)) {
		if (g_file_get_contents (path, &buffer, &length, &error)) {
			xmmsv_t *serialized;

			serialized = xmmsv_new_bin ((const guchar *) buffer, (guint) length);
			g_free (buffer);

			snapshot = xmmsv_deserialize (serialized);
			xmmsv_unref (serialized);
		}
	}

	if (error != NULL) {
		XMMS_DBG ("%s", error->message);
		g_error_free (error);
	}

	if (snapshot != NULL) {
		xmms_collection_restore (sync->dag, snapshot);
		xmmsv_unref (snapshot);
	} else {
		xmms_log_error ("Could not restore collections from disk.");
		xmms_collection_restore (sync->dag, NULL);
	}

	g_free (path);
}

/**
 * Wait until no collections have changed for 10 seconds, then sync.
 * @internal
 */
static gpointer
xmms_coll_sync_loop (gpointer udata)
{
	xmms_coll_sync_t *sync = (xmms_coll_sync_t *) udata;

	xmms_coll_sync_restore (sync);

	g_mutex_lock (&sync->mutex);

	while (sync->state != XMMS_COLL_SYNC_STATE_SHUTDOWN) {
		if (sync->state == XMMS_COLL_SYNC_STATE_IDLE) {
			g_cond_wait (&sync->cond, &sync->mutex);
		}

		/* Wait until no requests have been filed for 10 seconds. */
		while (sync->state == XMMS_COLL_SYNC_STATE_DELAYED) {
			gint64 end_time;

			sync->state = XMMS_COLL_SYNC_STATE_IDLE;

			end_time = g_get_monotonic_time () + XMMS_COLL_SYNC_DELAY;

			g_cond_wait_until (&sync->cond, &sync->mutex, end_time);
		}

		if (sync->state != XMMS_COLL_SYNC_STATE_SHUTDOWN) {
			/* The dag might be locked when calling schedule_sync, so we need to
			 * unlock to avoid deadlocks. We also reset state to IDLE to ack that
			 * the request to sync is being taken care of.
			 */
			sync->state = XMMS_COLL_SYNC_STATE_IDLE;

			g_mutex_unlock (&sync->mutex);
			xmms_coll_sync_save (sync);
			g_mutex_lock (&sync->mutex);
		}
	}

	g_mutex_unlock (&sync->mutex);

	xmms_coll_sync_save (sync);

	return NULL;
}
