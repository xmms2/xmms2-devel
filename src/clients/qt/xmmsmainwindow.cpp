
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

	setCentralWidget (box);
	resize (710, 500);

}
