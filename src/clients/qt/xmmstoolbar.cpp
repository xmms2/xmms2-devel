#include <qpushbutton.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qhbox.h>
#include <qdir.h>
#include <qfiledialog.h>
#include <qstringlist.h>

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-qt.h>

#include "xmmstoolbar.h"
#include "xmmsstatus.h"

XMMSToolbar::XMMSToolbar (XMMSClientQT *client, QWidget *parent) : QHBox (parent)
{
	QPushButton *button;

	m_client = client;
	
	XMMSStatus *st = new XMMSStatus (m_client, this);

	this->setStretchFactor (st, 15);

	button = new QPushButton (">", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onPlay ()));
	button = new QPushButton ("S", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onStop ()));
	button = new QPushButton ("->", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onNext ()));
	button = new QPushButton ("<-", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onPrev ()));
	button = new QPushButton ("^", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onAdd ()));
	button = new QPushButton ("Q", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onQuit ()));


}

void
XMMSToolbar::onPlay ()
{
	xmmsc_playback_start (m_client->getConnection ());
}

void
XMMSToolbar::onStop ()
{
	xmmsc_playback_stop (m_client->getConnection ());
}

void
XMMSToolbar::onNext ()
{
	xmmsc_play_next (m_client->getConnection ());
}

void
XMMSToolbar::onPrev ()
{
	xmmsc_play_prev (m_client->getConnection ());
}

void
XMMSToolbar::onAdd ()
{
	QStringList files = QFileDialog::getOpenFileNames("Audiofiles (*.mp3 *.sid *.ogg)",
							QDir::homeDirPath (),
							this,
							"select music"
							"Select one or more audiofiles to open" );
	
	QStringList list = files;
	QStringList::Iterator it = list.begin();
	while( it != list.end() ) {
		char tmp[1024];
		char *encoded;
		
		snprintf (tmp, 1024, "file://%s", (const char *)*it);
		encoded = xmmsc_encode_path (tmp);
		xmmsc_playlist_add (m_client->getConnection (), encoded);
		g_free (encoded);
		++it;
	}
}

void
XMMSToolbar::onQuit ()
{
	xmmsc_deinit (m_client->getConnection ());
	qApp->closeAllWindows ();
}
