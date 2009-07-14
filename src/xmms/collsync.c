/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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
#include "xmms/xmms_log.h"
#include <glib.h>

static GThread *thread;
static GMutex *mutex;
static GCond *cond;
static gboolean want_sync = FALSE;
static gboolean keep_running = TRUE;

/**
 * Wait until no collections have changed for 10 seconds, then sync.
 * @internal
 */
static gpointer
do_loop (gpointer udata)
{
	xmms_coll_dag_t *dag = udata;
	GTimeVal time;

	g_mutex_lock (mutex);

	while (keep_running) {
		if (!want_sync) {
			g_cond_wait (cond, mutex);
		}

		/* Wait until no requests have been filed for 10 seconds. */
		while (keep_running && want_sync) {
			want_sync = FALSE;

			g_get_current_time (&time);
			g_time_val_add (&time, 10000000);

			g_cond_timed_wait (cond, mutex, &time);
		}

		if (keep_running) {
			/* The dag might be locked when calling schedule_sync, so we need to
			 * unlock to avoid deadlocks */
			g_mutex_unlock (mutex);

			XMMS_DBG ("Syncing collections to database.");
			xmms_collection_sync (dag);

			g_mutex_lock (mutex);
		}
	}

	g_mutex_unlock (mutex);

	return NULL;
}

/**
 * Get the collection-to-database-synchronization thread running.
 */
void
xmms_coll_sync_init (xmms_coll_dag_t *dag)
{
	cond = g_cond_new ();
	mutex = g_mutex_new ();

	thread = g_thread_create (do_loop, dag, TRUE, NULL);
}

/**
 * Shutdown the collection-to-database-synchronization thread.
 */
void
xmms_coll_sync_shutdown ()
{
	g_mutex_lock (mutex);
	keep_running = FALSE;
	g_cond_signal (cond);
	g_mutex_unlock (mutex);

	g_thread_join (thread);

	g_mutex_free (mutex);
	g_cond_free (cond);
}

/**
 * Schedule a collection-to-database-synchronization in 10 seconds.
 */
void
xmms_coll_sync_schedule_sync ()
{
	g_mutex_lock (mutex);
	want_sync = TRUE;
	g_cond_signal (cond);
	g_mutex_unlock (mutex);
}
