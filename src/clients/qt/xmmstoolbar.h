#ifndef __XMMSTOOLBAR_H__
#define __XMMSTOOLBAR_H__

#include <qlayout.h>
#include <qhbox.h>

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-qt.h>

class XMMSToolbar : public QHBox
{
	Q_OBJECT
public:
	XMMSToolbar (XMMSClientQT *, QWidget *);

protected:
	XMMSClientQT *m_client;

protected slots:
	void onPlay ();
	void onStop ();
	void onNext ();
	void onPrev ();
	void onAdd ();
	void onQuit ();

};

#endif

