#include <stdlib.h>

#include <qapplication.h>

#include <xmmsclient.h>

#include "xmmsplaylistwin.h"
#include "xmmslistview.h"
#include "xmms/signal_xmms.h"
#include "playlist.h"

static void
playlist_list (void *userdata, void *arg)
{
	XMMSPlaylistWin *pl = (XMMSPlaylistWin *) userdata;
	guint32 *list = (guint32 *) arg;
	guint i=0;

	pl->clear ();

	while (list[i]) {
		pl->add (list[i]);
		i++;
	}
}

static void
playlist_mediainfo (void *userdata, void *arg)
{
	XMMSPlaylistWin *pl = (XMMSPlaylistWin *) userdata;
	pl->setInfo ((GHashTable *)arg);
}

XMMSPlaylistWin::XMMSPlaylistWin (xmmsc_connection_t *conn) : 
			QMainWindow (0, "XMMSPlaylist", WDestructiveClose)
{

	setCaption ("XMMSPlaylist");

	m_listview = new XMMSListView (this, NULL);
	m_listview->setRootIsDecorated (TRUE);
	m_connection = conn;

	m_playlist = playlist_init (conn);

	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_LIST, playlist_list, this);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_MEDIAINFO, playlist_mediainfo, this);

	xmmsc_playlist_list (conn);

	setCentralWidget (m_listview);
	resize (710, 500);
}

void
XMMSPlaylistWin::clear ()
{
	m_listview->clear ();
}

void
XMMSPlaylistWin::add (unsigned int id)
{
	XMMSListViewItem *it;

	qDebug ("adding %d", id);
	it = new XMMSListViewItem (m_listview, id);
	playlist_set_id_data (m_playlist, id, it);
}

void
XMMSPlaylistWin::setInfo (GHashTable *t)
{
	XMMSListViewItem *i;
	guint id;
	char *tmp;

	id = GPOINTER_TO_UINT (g_hash_table_lookup (t, "id"));

	qDebug ("updating info for %d", id);

	i = (XMMSListViewItem *) playlist_get_id_data (m_playlist, id);
	i->setArtist ((char *)g_hash_table_lookup (t, "artist"));
	i->setAlbum ((char *)g_hash_table_lookup (t, "album"));
	i->setTitle ((char *)g_hash_table_lookup (t, "title"));
	i->setURL ((char *)g_hash_table_lookup (t, "url"));
	tmp = (char *)g_hash_table_lookup (t, "duration");
	if (tmp)
		i->setDuration (atoi (tmp));
	else
		i->setDuration (0);
}
