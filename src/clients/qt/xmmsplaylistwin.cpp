#include <stdlib.h>

#include <qapplication.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qstatusbar.h>

#include <xmmsclient.h>

#include "xmmsplaylistwin.h"
#include "xmmstoolbar.h"
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

	while (list && list[i]) {
		pl->add (list[i]);
		i++;
		qApp->processEvents ();
	}
}

static void
playlist_mediainfo (void *userdata, void *arg)
{
	XMMSPlaylistWin *pl = (XMMSPlaylistWin *) userdata;
	pl->setInfo ((GHashTable *)arg);
}

static void
playlist_mediainfo_id (void *userdata, void *arg)
{
	XMMSPlaylistWin *pl = (XMMSPlaylistWin *) userdata;
	pl->getInfo (GPOINTER_TO_UINT (arg));
}

static void
playback_currentid (void *userdata, void *arg)
{
	XMMSPlaylistWin *pl = (XMMSPlaylistWin *) userdata;
	pl->setCurrentId (GPOINTER_TO_UINT (arg));
}

static void
playlist_add (void *userdata, void *arg)
{
	XMMSPlaylistWin *pl = (XMMSPlaylistWin *) userdata;
	pl->add (GPOINTER_TO_UINT (arg));
}

static void
playlist_clear (void *userdata, void *arg)
{
	XMMSPlaylistWin *pl = (XMMSPlaylistWin *) userdata;
	pl->clear ();
}

XMMSPlaylistWin::XMMSPlaylistWin (xmmsc_connection_t *conn) : 
			QMainWindow (0, "XMMSPlaylist", WDestructiveClose)
{
	QVBox *box;
	XMMSToolbar *toolbar;

	setCaption ("XMMSPlaylist");

	box = new QVBox (this);

	m_listview = new XMMSListView (conn, box, NULL);
	m_listview->setRootIsDecorated (TRUE);
	m_connection = conn;

	m_playlist = playlist_init (conn);

	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYBACK_CURRENTID, playback_currentid, this);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_LIST, playlist_list, this);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_MEDIAINFO, playlist_mediainfo, this);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID, playlist_mediainfo_id, this);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_ADD, playlist_add, this);
	xmmsc_set_callback (conn, XMMS_SIGNAL_PLAYLIST_CLEAR, playlist_clear, this);

	xmmsc_playlist_list (conn);

	/* toolbar */
	toolbar = new XMMSToolbar (m_connection, box);

	m_currentItem = NULL;

	setCentralWidget (box);
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
	QListViewItem *l = m_listview->lastItem ();

	qDebug ("adding %d", id);
	it = new XMMSListViewItem (m_listview, id, l);
	playlist_set_id_data (m_playlist, id, it);

	/* Get the information about this entry */
	xmmsc_playlist_get_mediainfo (m_connection, id);
}

void
XMMSPlaylistWin::getInfo (guint id)
{
	xmmsc_playlist_get_mediainfo (m_connection, id);
}

void
XMMSPlaylistWin::setCurrentId (guint id)
{
	XMMSListViewItem *i;

	i = (XMMSListViewItem *) playlist_get_id_data (m_playlist, id);
	
	if (m_currentItem)
		m_currentItem->setCurrent (FALSE);

	i->setCurrent (TRUE);

	m_currentItem = i;
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
	i->setURL (xmmsc_decode_path ((char *)g_hash_table_lookup (t, "url")));
	tmp = (char *)g_hash_table_lookup (t, "duration");
	if (tmp)
		i->setDuration (atoi (tmp));
	else
		i->setDuration (0);


	m_listview->repaintItem (i);
}
