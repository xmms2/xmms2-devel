#include <qapplication.h>

#include "xmmsplaylistwin.h"
#include "xmmsclient.h"
#include "xmmsclient-qt.h"

int main (int argc, char **argv)
{
	xmmsc_connection_t *c;
	QApplication app (argc, argv);
	XMMSClientQT *client;

	c = xmmsc_init ();

	if (!xmmsc_connect (c, "unix:path=/tmp/xmms-dbus-tru"))
		return 0;
	
	client = new XMMSClientQT (c, &app);

	XMMSPlaylistWin *pl = new XMMSPlaylistWin (c);

	app.setMainWidget (pl);

	pl->show ();
	
	return app.exec ();

}

