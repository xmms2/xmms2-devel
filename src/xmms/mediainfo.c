/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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
 * This file controls the mediainfo reader thread.
 *
 */

#include <stdlib.h>

#include <xmms/xmms_log.h>
#include <xmms/xmms_ipc.h>
#include <xmmspriv/xmms_mediainfo.h>
#include <xmmspriv/xmms_medialib.h>
#include <xmmspriv/xmms_xform.h>


#include <glib.h>

/** @defgroup MediaInfoReader MediaInfoReader
  * @ingroup XMMSServer
  * @brief The mediainfo reader.
  *
  * When a item is added to the playlist the mediainfo reader will
  * start extracting the information from this entry and update it
  * if additional information is found.
  * @{
  */

struct xmms_mediainfo_reader_St {
	xmms_object_t object;

	GThread *thread;
	GMutex mutex;
	GCond cond;

	gboolean running;

	xmms_medialib_t *medialib;
};

static void xmms_mediainfo_reader_stop (xmms_object_t *o);
static gpointer xmms_mediainfo_reader_thread (gpointer data);

#include "mediainfo_ipc.c"

static void
on_medialib_entry_added (xmms_object_t *object, xmmsv_t *val, gpointer udata)
{
	xmms_mediainfo_reader_t *mrt = (xmms_mediainfo_reader_t *) udata;
	xmms_mediainfo_reader_wakeup (mrt);
}

/**
 * Start the mediainfo reader thread
 */
xmms_mediainfo_reader_t *
xmms_mediainfo_reader_start (xmms_medialib_t *medialib)
{
	xmms_mediainfo_reader_t *mrt;

	mrt = xmms_object_new (xmms_mediainfo_reader_t,
	                       xmms_mediainfo_reader_stop);

	xmms_mediainfo_reader_register_ipc_commands (XMMS_OBJECT (mrt));

	g_mutex_init (&mrt->mutex);
	g_cond_init (&mrt->cond);
	mrt->running = TRUE;
	mrt->thread = g_thread_new ("x2 media info", xmms_mediainfo_reader_thread, mrt);

	xmms_object_ref (medialib);
	mrt->medialib = medialib;


	xmms_object_connect (XMMS_OBJECT (mrt->medialib),
	                     XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_ADDED,
	                     on_medialib_entry_added, mrt);

	xmms_object_connect (XMMS_OBJECT (mrt->medialib),
	                     XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_CHANGED,
	                     on_medialib_entry_added, mrt);

	return mrt;
}

/**
  * Kill the mediainfo reader thread
  */
static void
xmms_mediainfo_reader_stop (xmms_object_t *o)
{
	xmms_mediainfo_reader_t *mir = (xmms_mediainfo_reader_t *) o;

	XMMS_DBG ("Deactivating mediainfo object.");

	g_mutex_lock (&mir->mutex);
	mir->running = FALSE;
	g_cond_signal (&mir->cond);
	g_mutex_unlock (&mir->mutex);

	xmms_mediainfo_reader_unregister_ipc_commands ();

	g_thread_join (mir->thread);

	g_cond_clear (&mir->cond);
	g_mutex_clear (&mir->mutex);

	xmms_object_disconnect (XMMS_OBJECT (mir->medialib),
	                        XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_CHANGED,
	                        on_medialib_entry_added, mir);

	xmms_object_disconnect (XMMS_OBJECT (mir->medialib),
	                        XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_ADDED,
	                        on_medialib_entry_added, mir);

	xmms_object_unref (mir->medialib);
}

/**
 * Wake the reader thread and start process the entries.
 */

void
xmms_mediainfo_reader_wakeup (xmms_mediainfo_reader_t *mr)
{
	g_return_if_fail (mr);

	g_mutex_lock (&mr->mutex);
	g_cond_signal (&mr->cond);
	g_mutex_unlock (&mr->mutex);
}

/** @} */

static gpointer
xmms_mediainfo_reader_thread (gpointer data)
{
	GList *goal_format;
	GTimeVal timeval;
	xmms_stream_type_t *f;
	guint num = 0;

	xmms_mediainfo_reader_t *mrt = (xmms_mediainfo_reader_t *) data;

	xmms_object_emit (XMMS_OBJECT (mrt),
	                  XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
	                  xmmsv_new_int (XMMS_MEDIAINFO_READER_STATUS_RUNNING));

	f = _xmms_stream_type_new (XMMS_STREAM_TYPE_BEGIN,
	                           XMMS_STREAM_TYPE_MIMETYPE,
	                           "audio/pcm",
	                           XMMS_STREAM_TYPE_END);
	goal_format = g_list_prepend (NULL, f);

	while (mrt->running) {
		xmmsc_medialib_entry_status_t prev_status;
		xmms_medialib_entry_t entry;
		xmms_medialib_session_t *session;
		xmms_xform_t *xform;

		session = xmms_medialib_session_begin (mrt->medialib);
		entry = xmms_medialib_entry_not_resolved_get (session);
		XMMS_DBG ("got %d as not resolved", entry);

		if (!entry) {
			xmms_medialib_session_abort (session);
			xmms_object_emit (XMMS_OBJECT (mrt),
			                  XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
			                  xmmsv_new_int (XMMS_MEDIAINFO_READER_STATUS_IDLE));

			g_mutex_lock (&mrt->mutex);
			g_cond_wait (&mrt->cond, &mrt->mutex);
			g_mutex_unlock (&mrt->mutex);

			num = 0;

			xmms_object_emit (XMMS_OBJECT (mrt),
			                  XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
			                  xmmsv_new_int (XMMS_MEDIAINFO_READER_STATUS_RUNNING));
			continue;
		}

		prev_status = xmms_medialib_entry_property_get_int (session, entry,
		                                                    XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS);

		if (num == 0) {
			xmms_object_emit (XMMS_OBJECT (mrt),
			                  XMMS_IPC_SIGNAL_MEDIAINFO_READER_UNINDEXED,
			                  xmmsv_new_int (xmms_medialib_num_not_resolved (session)));
			num = 10;
		} else {
			num--;
		}

		xform = xmms_xform_chain_setup_session (mrt->medialib, session, entry,
		                                        goal_format, TRUE);

		if (!xform) {
			if (prev_status == XMMS_MEDIALIB_ENTRY_STATUS_NEW) {
				xmms_medialib_entry_remove (session, entry);
			} else {
				xmms_medialib_entry_status_set (session, entry,
				                                XMMS_MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE);
			}
		} else {
			xmms_object_unref (xform);
			g_get_current_time (&timeval);

			xmms_medialib_entry_property_set_int (session, entry,
			                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ADDED,
			                                      timeval.tv_sec);
		}
		xmms_medialib_session_commit (session);
	}

	g_list_free (goal_format);
	xmms_object_unref (f);

	return NULL;
}
