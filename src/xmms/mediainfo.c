/** @file This file controls the mediainfothread.
  *
  */

#include "xmms/xmms.h"
#include "xmms/transport.h"
#include "xmms/transport_int.h"
#include "xmms/decoder.h"
#include "xmms/decoder_int.h"
#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/mediainfo.h"

#include <glib.h>

struct xmms_mediainfo_thread_St {
	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	gboolean running;
	GList *list;
	xmms_playlist_t *playlist;
	/* 45574 */
};

static gpointer xmms_mediainfo_thread_thread (gpointer data);

xmms_mediainfo_thread_t *
xmms_mediainfo_thread_start (xmms_playlist_t *playlist)
{
	xmms_mediainfo_thread_t *mtt;

	g_return_val_if_fail (playlist, NULL);

	mtt = g_new0 (xmms_mediainfo_thread_t, 1);

	mtt->mutex = g_mutex_new ();
	mtt->cond = g_cond_new ();
	mtt->playlist = playlist;

	mtt->running = TRUE;
	mtt->thread = g_thread_create (xmms_mediainfo_thread_thread, mtt, FALSE, NULL);

	return mtt;
}

void
xmms_mediainfo_thread_add (xmms_mediainfo_thread_t *mthread, guint entryid)
{
	g_mutex_lock (mthread->mutex);

	mthread->list = g_list_append (mthread->list, GUINT_TO_POINTER (entryid));

	g_cond_signal (mthread->cond);

	g_mutex_unlock (mthread->mutex);

}

static gpointer
xmms_mediainfo_thread_thread (gpointer data)
{
	xmms_mediainfo_thread_t *mtt = (xmms_mediainfo_thread_t *) data;
	GList *node = NULL;

	while (mtt->running) {
		xmms_playlist_entry_t *entry;
		
		g_mutex_lock (mtt->mutex);

		XMMS_DBG ("MediainfoThread is idle.");
		g_cond_wait (mtt->cond, mtt->mutex);

		XMMS_DBG ("MediainfoThread is awake!");

		while ((node = g_list_nth (mtt->list, 0))) {
			xmms_transport_t *transport;
			xmms_playlist_entry_t *newentry;
			xmms_decoder_t *decoder;
			const gchar *mime;
			const gchar *uri;
			guint id;

			mtt->list = g_list_remove_link (mtt->list, node);

			id = GPOINTER_TO_UINT (node->data);

			g_list_free_1 (node);

			g_mutex_unlock (mtt->mutex);

			entry = xmms_playlist_get_byid (mtt->playlist, id);

			uri = xmms_playlist_entry_get_uri (entry);
			transport = xmms_transport_open (uri);
			if (!transport) {
				xmms_playlist_entry_unref (entry);
				continue;
			}

			mime = xmms_transport_mime_type_get (transport);
			if (mime) {
				xmms_playlist_entry_mimetype_set (entry, mime);
				decoder = xmms_decoder_new (entry);
				if (!decoder) {
					xmms_playlist_entry_unref (entry);
					xmms_transport_close (transport);
					continue;
				}
			} else {
				xmms_playlist_entry_unref (entry);
				xmms_transport_close (transport);
				continue;
			}

			/*xmms_transport_start (transport);*/

			newentry = xmms_decoder_get_mediainfo (decoder, transport);
			if (newentry) {
				XMMS_DBG ("updating %s", xmms_playlist_entry_get_uri (newentry));
				//xmms_playlist_entry_copy_property (newentry, entry);
			}

			xmms_playlist_entry_unref (entry);
			xmms_transport_close (transport);
			xmms_decoder_destroy (decoder);
				

			g_mutex_lock (mtt->mutex);

		}

		g_mutex_unlock (mtt->mutex);

	}


	return NULL;
}

