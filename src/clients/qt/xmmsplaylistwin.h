#ifndef __XMMSPLAYLISTWIN_H__
#define __XMMSPLAYLISTWIN_H__

#include <qapplication.h>
#include <qmainwindow.h>
#include "xmmslistview.h"
#include "playlist.h"

class XMMSPlaylistWin : public QMainWindow
{
	Q_OBJECT

public:
	XMMSPlaylistWin (xmmsc_connection_t *);
	void setInfo (GHashTable *);
	void add (unsigned int);
	void clear ();

protected:
	XMMSListView *m_listview;
	playlist_t *m_playlist;
	xmmsc_connection_t *m_connection;
};

#endif

