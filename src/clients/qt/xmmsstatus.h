#ifndef __XMMSSTATUS_H__
#define __XMMSSTATUS_H__

#include <qwidget.h>
#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-qt.h>

class XMMSStatus : public QWidget 
{

public:
	XMMSStatus (XMMSClientQT *, QWidget *);
	QSize sizeHint () const;

protected:
	void paintEvent (QPaintEvent *);

	XMMSClientQT *m_client;
	QString *m_str;
	QString *m_tme;
};

#endif
