#include <qpushbutton.h>
#include <qapplication.h>
#include <qhbox.h>
#include <qdir.h>
#include <qfiledialog.h>
#include <qstringlist.h>

#include <xmmsclient.h>

#include "xmmstoolbar.h"

XMMSToolbar::XMMSToolbar (xmmsc_connection_t *conn, QWidget *parent) : QHBox (parent)
{
	QPushButton *button;

	m_connection = conn;

	button = new QPushButton ("Play", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onPlay ()));
	button = new QPushButton ("Stop", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onStop ()));
	button = new QPushButton ("Next", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onNext ()));
	button = new QPushButton ("Prev", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onPrev ()));
	button = new QPushButton ("Add", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onAdd ()));
	button = new QPushButton ("Clear", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onClear ()));
	button = new QPushButton ("Quit", this);
	connect (button, SIGNAL (clicked ()), this, SLOT (onQuit ()));

}

void
XMMSToolbar::onPlay ()
{
	xmmsc_playback_start (m_connection);
}

void
XMMSToolbar::onStop ()
{
	xmmsc_playback_stop (m_connection);
}

void
XMMSToolbar::onNext ()
{
	xmmsc_play_next (m_connection);
}

void
XMMSToolbar::onPrev ()
{
	xmmsc_play_prev (m_connection);
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
		xmmsc_playlist_add (m_connection, encoded);
		g_free (encoded);
		++it;
	}
}

void
XMMSToolbar::onClear ()
{
	xmmsc_playlist_clear (m_connection);
}

void
XMMSToolbar::onQuit ()
{
	xmmsc_deinit (m_connection);
	qApp->closeAllWindows ();
}
