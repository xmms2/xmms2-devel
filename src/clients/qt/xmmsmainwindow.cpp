
#include <stdlib.h>

#include <qapplication.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qstatusbar.h>
#include <qprogressbar.h>

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
	XMMSListViewItem *citem;
	
	qDebug ("id: %d", id);
	if (id != 0) {
		conn = mw->client ()->getConnection ();
		xmmsc_playlist_get_mediainfo (conn, id);
	}

	citem = (XMMSListViewItem *) x_hash_lookup (mw->idHash (), arg);

	if (citem) {
		if (mw->cItem ()) {
			mw->cItem ()->setCurrent (FALSE);
		}
		citem->setCurrent (TRUE);
		mw->setcItem (citem);
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

	if (!mw->barBusy ()) {
		if (mw->cItem ()) {
			mw->bar ()->setTotalSteps (mw->cItem ()->duration ());
		}
		mw->bar ()->setProgress (dur);
	}

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

	mw->setBarBusy (TRUE);
	mw->bar ()->setTotalSteps (i);
	mw->bartxt ()->setText ("Reading playlist ...");

	qDebug ("%d entries in playlist", i);

	while (list && list[j]) {
		mw->add (list[j]);
		j++;
		mw->bar ()->setProgress (j);
		qApp->processEvents ();
	}

	mw->bar ()->reset ();
	mw->setBarBusy (FALSE);
	mw->bartxt ()->setText ("Waiting for events");
	xmmsc_playback_current_id (mw->client ()->getConnection ());
}

static void
handle_playlist_mediainfo_id (void *userdata, void *arg)
{
	XMMSMainWindow *mw = (XMMSMainWindow *)userdata;
	printf ("Got id %d\n", XPOINTER_TO_INT (arg));
	xmmsc_playlist_get_mediainfo (mw->client ()->getConnection (), XPOINTER_TO_INT (arg));
}

static void
handle_playlist_add (void *userdata, void *arg)
{
	XMMSMainWindow *mw = (XMMSMainWindow *)userdata;
	mw->add (XPOINTER_TO_INT (arg));
}

static void
handle_playlist_mediainfo (void *userdata, void *arg)
{
        x_hash_t *tab = (x_hash_t *)arg;
	XMMSMainWindow *mw = (XMMSMainWindow *)userdata;
	XMMSListViewItem *it;
        unsigned int id;
	char d[9];
        char *artist, *album, *title, *channel;
	char fstr[1024];
	char *t;
	int dur;

        if (!arg)
                return;

        id = XPOINTER_TO_UINT (x_hash_lookup (tab, "id"));


        t = (char *)x_hash_lookup (tab, "duration");

	if (t) {
		dur = atoi (t);	
		snprintf (d, 9, "%02d:%02d", dur/60000, (dur/1000)%60);
	} else {
		dur = 0;
		snprintf (d, 9, "00:00");
	}

	it = (XMMSListViewItem *)x_hash_lookup (mw->idHash (), XUINT_TO_POINTER (id));


	if (!it) {
		printf ("Apan\n");
	} else {
		it->setArtist ((char *)x_hash_lookup (tab, "artist"));
		it->setTitle ((char *)x_hash_lookup (tab, "title"));
		it->setAlbum ((char *)x_hash_lookup (tab, "album"));
		it->setDuration (dur);
		it->repaint ();
	}

	
        if (id == mw->ID()) {
		QString *s;
		
		mw->toolbar ()->setTME (new QString (d));

		memset (fstr, 0, 1024);
		xmmsc_entry_format (fstr, 1024, "%a - %b - %t", tab);

		mw->toolbar ()->setText (new QString (fstr));
		mw->bartxt ()->setText (QString ("Playing ") + QString (fstr));
		if (!mw->barBusy ())
			mw->bar ()->setTotalSteps (dur);
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
	m_bartxt = new QLabel (statusBar ());
	statusBar ()->addWidget (m_bartxt, 2, FALSE);
	m_bar = new QProgressBar (statusBar ());
	m_barbusy = FALSE;
	statusBar ()->addWidget (m_bar, 1, TRUE);

	m_citem = NULL;

	/* id hash table */
	m_idhash = x_hash_new (x_direct_hash, x_direct_equal);
	
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYBACK_CURRENTID, handle_currentid, (void*)this);
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYBACK_PLAYTIME, handle_playtime, (void*)this);
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYLIST_LIST, handle_playlist_list, (void*)this);
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYLIST_MEDIAINFO, handle_playlist_mediainfo, (void*)this);
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID, handle_playlist_mediainfo_id, (void*)this);
	xmmsc_set_callback (m_client->getConnection (), XMMS_SIGNAL_PLAYLIST_ADD, handle_playlist_add, (void*)this);

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
	x_hash_insert (m_idhash, XUINT_TO_POINTER (id), (void *)it);
	xmmsc_playlist_get_mediainfo (m_client->getConnection (), id);
}
