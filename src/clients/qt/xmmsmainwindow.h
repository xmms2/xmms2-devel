#ifndef __XMMSPLAYLISTWIN_H__
#define __XMMSPLAYLISTWIN_H__

#include <qapplication.h>
#include <qmainwindow.h>
#include <qstatusbar.h>
#include "xmmslistview.h"
#include "playlist.h"

class XMMSPlaylistWin : public QMainWindow
{
	Q_OBJECT

public:
	XMMSPlaylistWin (xmmsc_connection_t *);
	void setCurrentId (guint);
	void setInfo (GHashTable *);
	void getInfo (guint);
	void add (unsigned int);
	void clear ();

protected:
	XMMSListView *m_listview;
	playlist_t *m_playlist;
	xmmsc_connection_t *m_connection;
	QStatusBar *m_status;
	XMMSListViewItem *m_currentItem;
};

#endif

