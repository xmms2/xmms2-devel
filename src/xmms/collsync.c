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


/** @file
 *  Manages the synchronization of collections to the database at 10 seconds
 *  after the last collections-change.
 */

#include "xmmspriv/xmms_collsync.h"
#include "xmmspriv/xmms_collserial.h"
#include "xmmspriv/xmms_thread_name.h"
#include "xmms/xmms_log.h"
#include <glib.h>

static void xmms_coll_sync_schedule_sync (xmms_object_t *object, xmmsv_t *val, gpointer udata);
static gpointer xmms_coll_sync_loop (gpointer udata);
static void xmms_coll_sync_destroy (xmms_object_t *object);

static void xmms_coll_sync_start (xmms_coll_sync_t *sync);
static void xmms_coll_sync_stop (xmms_coll_sync_t *sync);

struct xmms_coll_sync_St {
	xmms_object_t object;

	xmms_coll_dag_t *dag;
	xmms_playlist_t *playlist;

	gchar *uuid;

	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	gboolean want_sync;
	gboolean keep_running;
};

/**
 * Get the collection-to-database-synchronization thread running.
 */
xmms_coll_sync_t *
xmms_coll_sync_init (const gchar *uuid, xmms_coll_dag_t *dag, xmms_playlist_t *playlist)
{
	xmms_coll_sync_t *sync;

	sync = xmms_object_new (xmms_coll_sync_t, xmms_coll_sync_destroy);

	sync->uuid = g_strdup (uuid);

	sync->cond = g_cond_new ();
	sync->mutex = g_mutex_new ();

	xmms_object_ref (dag);
	sync->dag = dag;

	xmms_object_ref (playlist);
	sync->playlist = playlist;

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

	g_mutex_free (sync->mutex);
	g_cond_free (sync->cond);
	g_free (sync->uuid);
}

static void
xmms_coll_sync_start (xmms_coll_sync_t *sync)
{
	g_return_if_fail (sync);
	g_return_if_fail (sync->thread == NULL);

	sync->want_sync = FALSE;
	sync->keep_running = TRUE;
	sync->thread = g_thread_create (xmms_coll_sync_loop, sync, TRUE, NULL);
}


/**
 * Signal the sync loop to exit and join the sync thread.
 */
static void
xmms_coll_sync_stop (xmms_coll_sync_t *sync)
{
	g_return_if_fail (sync);
	g_return_if_fail (sync->thread != NULL);

	g_mutex_lock (sync->mutex);

	sync->keep_running = FALSE;
	g_cond_signal (sync->cond);

	g_mutex_unlock (sync->mutex);

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

	g_mutex_lock (sync->mutex);
	sync->want_sync = TRUE;
	g_cond_signal (sync->cond);
	g_mutex_unlock (sync->mutex);
}

/**
 * Wait until no collections have changed for 10 seconds, then sync.
 * @internal
 */
static gpointer
xmms_coll_sync_loop (gpointer udata)
{
	xmms_coll_sync_t *sync = (xmms_coll_sync_t *) udata;
	GTimeVal time;

	xmms_set_thread_name ("x2 coll sync");

	g_mutex_lock (sync->mutex);

	xmms_collection_dag_restore (sync->dag, sync->uuid);

	while (sync->keep_running) {
		if (!sync->want_sync) {
			g_cond_wait (sync->cond, sync->mutex);
		}

		/* Wait until no requests have been filed for 10 seconds. */
		while (sync->keep_running && sync->want_sync) {
			sync->want_sync = FALSE;

			g_get_current_time (&time);
			g_time_val_add (&time, 10000000);

			g_cond_timed_wait (sync->cond, sync->mutex, &time);
		}

		if (sync->keep_running) {
			/* The dag might be locked when calling schedule_sync, so we need to
			 * unlock to avoid deadlocks */
			g_mutex_unlock (sync->mutex);

			XMMS_DBG ("Syncing collections to database.");

			xmms_collection_dag_save (sync->dag, sync->uuid);

			g_mutex_lock (sync->mutex);
		}
	}

	xmms_collection_dag_save (sync->dag, sync->uuid);

	g_mutex_unlock (sync->mutex);

	return NULL;
}
