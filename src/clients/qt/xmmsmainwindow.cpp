
#include <stdlib.h>

#include <qapplication.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qstatusbar.h>

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-qt.h>

#include "xmmsmainwindow.h"
#include "xmmstoolbar.h"
#include "xmmslistview.h"
#include "xmms/signal_xmms.h"

#include "internal/xhash-int.h"

static void
handle_currentid (void *userdata, void *arg) {
	int id = XPOINTER_TO_UINT (arg);
	xmmsc_connection_t *conn;
	XMMSMainWindow *mw = (XMMSMainWindow *)userdata;
	
	qDebug ("id: %d", id);
	if (id != 0) {
		conn = mw->client ()->getConnection ();
		xmmsc_playlist_get_mediainfo (conn, id);
	}
	mw->setID (id);
}

static void
handle_playtime (void *userdata, void *arg) 
{
	int dur = XPOINTER_TO_UINT (arg);
	char d[9];
	QString *s;
	XMMSMainWindow *mw = (XMMSMainWindow *)userdata;

	snprintf (d, 9, "%02d:%02d", dur/60000, (dur/1000)%60);
	s = new QString (d);
	if (*s != *mw->toolbar ()->status ()->currentTME ()) 
		mw->toolbar ()->setCTME (s);

}

static void
handle_playlist_list (void *userdata, void *arg)
{
	XMMSMainWindow *mw = (XMMSMainWindow *)userdata;
	int *list = (int *) arg;
	int i = 0, j = 0;

	mw->pl ()->clear ();

	while (list && list[i])
		i++;

	qDebug ("%d entries in playlist", i);

	while (list && list[j]) {
		mw->add (list[j]);
		j++;
		qApp->processEvents ();
	}
}

static void
handle_playlist_mediainfo (void *userdata, void *arg)
{
        x_hash_t *tab = (x_hash_t *)arg;
	XMMSMainWindow *mw = (XMMSMainWindow *)userdata;
        unsigned int id;
	char d[9];
        char *artist, *album, *title, *channel;
	char fstr[1024];
	char *t;
	int dur;

        if (!arg)
                return;

        id = XPOINTER_TO_UINT (x_hash_lookup (tab, "id"));
	
        if (id == mw->ID()) {
		QString *s;
		
                t = (char *)x_hash_lookup (tab, "duration");

		if (t) {
			dur = atoi (t);	
			snprintf (d, 9, "%02d:%02d", dur/60000, (dur/1000)%60);
		} else {
			snprintf (d, 9, "00:00");
		}

		mw->toolbar ()->setTME (new QString (d));

		xmmsc_entry_format (fstr, 1024, "%a - %b - %t", tab);

		mw->toolbar ()->setText (new QString (fstr));

        } else {
	        t = (char *)x_hash_lookup (tab, "duration");

		if (t) {
			dur = atoi (t);	
			snprintf (d, 9, "%02d:%02d", dur/60000, (dur/1000)%60);
		} else {
			snprintf (d, 9, "00:00");
		}

	}
                                                                                                                         
        xmmsc_playlist_entry_free (tab);
}


XMMSMainWindow::XMMSMainWindow (XMMSClientQT *client) : 
			QMainWindow (0, "XMMSMainWindow", WDestructiveClose)
{
	QVBox *box;
	XMMSToolbar *toolbar;

	m_client = client;

	setCaption ("XMMS2 - QTClient");

	box = new QVBox (this);

	/* toolbar */
	m_toolbar = new XMMSToolbar (m_client, box);

	/* playlist listview */
	m_listview = new XMMSListView (m_client, box, NULL);
	
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYBACK_CURRENTID, handle_currentid, (void*)this);
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYLIST_MEDIAINFO, handle_playlist_mediainfo, (void*)this);
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYBACK_PLAYTIME, handle_playtime, (void*)this);
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYLIST_LIST, handle_playlist_list, (void*)this);

	xmmsc_playback_current_id (m_client->getConnection ());
	xmmsc_playlist_list (m_client->getConnection ());

	setCentralWidget (box);
	resize (710, 500);

}

void
XMMSMainWindow::add (int id)
{
	XMMSListViewItem *it;
	QListViewItem *l = m_listview->lastItem ();

	it = new XMMSListViewItem (m_listview, id, l);
	xmmsc_playlist_get_mediainfo (m_client->getConnection (), id);
}


