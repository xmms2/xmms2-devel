#ifndef __XMMSTOOLBAR_H__
#define __XMMSTOOLBAR_H__

#include <qhbox.h>

#include <xmmsclient.h>

class XMMSToolbar : public QHBox
{
	Q_OBJECT
public:
	XMMSToolbar (xmmsc_connection_t *, QWidget *);

protected:
	xmmsc_connection_t *m_connection;

protected slots:
	void onPlay ();
	void onStop ();
	void onNext ();
	void onPrev ();
	void onAdd ();
	void onClear ();
	void onQuit ();

};

#endif

