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
#include "xmms/plsplugins.h"

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
			xmms_playlist_plugin_t *plsplugin;
			xmms_decoder_t *decoder;
			const gchar *mime;
			guint id;

			mtt->list = g_list_remove_link (mtt->list, node);

			id = GPOINTER_TO_UINT (node->data);

			g_list_free_1 (node);

			g_mutex_unlock (mtt->mutex);

			entry = xmms_playlist_get_byid (mtt->playlist, id);

			transport = xmms_transport_new ();
			xmms_transport_open (transport, entry);
			if (!transport) {
				xmms_playlist_entry_unref (entry);
				continue;
			}

			if (!xmms_transport_islocal (transport))
				continue;

			mime = xmms_transport_mimetype_get (transport);

			if (!mime) {
				xmms_playlist_entry_unref (entry);
				xmms_transport_close (transport);
				continue;
			}

			plsplugin = xmms_playlist_plugin_new (mime);
			if (plsplugin) {

				/* This is a playlist file... */
				XMMS_DBG ("Playlist!!");
				xmms_playlist_plugin_read (plsplugin, mtt->playlist, transport);

				/* we don't want it in the playlist. */
				xmms_playlist_id_remove (mtt->playlist, xmms_playlist_entry_id_get (entry));
				xmms_playlist_entry_unref (entry);

				/* cleanup */
				xmms_playlist_plugin_free (plsplugin);
				xmms_transport_close (transport);
				continue;
			}


			xmms_playlist_entry_mimetype_set (entry, mime);
			decoder = xmms_decoder_new ();
			if (xmms_decoder_open (decoder, entry)) {
				xmms_playlist_entry_unref (entry);
				xmms_transport_close (transport);
				xmms_decoder_destroy (decoder);
				continue;
			}

			/*xmms_transport_start (transport);*/

			newentry = xmms_decoder_mediainfo_get (decoder, transport);
			if (newentry) {
				XMMS_DBG ("updating %s", xmms_playlist_entry_url_get (newentry));
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

