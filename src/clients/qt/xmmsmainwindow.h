#ifndef __XMMSMAINWINDOW_H__
#define __XMMSMAINWINDOW_H__

#include <qapplication.h>
#include <qmainwindow.h>
#include <qstatusbar.h>
#include <xmms/xmmsclient-qt.h>

#include "xmmslistview.h"
#include "xmmstoolbar.h"

class XMMSMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	XMMSMainWindow (XMMSClientQT *);
	void setCurrentId (guint);
	void setInfo (GHashTable *);
	void getInfo (guint);
	void add (unsigned int);
	void clear ();

protected:
	XMMSListView *m_listview;
	XMMSToolbar *m_toolbar;
	//XMMSStatusBar *m_statusbar;
	
	XMMSClientQT *m_client;
};

#endif

