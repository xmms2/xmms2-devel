#include <qapplication.h>

#include "xmmsmainwindow.h"
#include "xmms/xmmsclient.h"
#include "xmms/xmmsclient-qt.h"

int main (int argc, char **argv)
{
	xmmsc_connection_t *c;
	QApplication app (argc, argv);
	XMMSClientQT *client;

	c = xmmsc_init ();

	if (!xmmsc_connect (c, NULL))
		return 0;
	
	/* Here goes the Magic, this will create the
	 * actuall cooperation between the mainloop and
	 * dbus. 
	 */
	client = new XMMSClientQT (c, &app);

	XMMSMainWindow *mw = new XMMSMainWindow (client);

	app.setMainWidget (mw);

	mw->show ();
	
	return app.exec ();

}

