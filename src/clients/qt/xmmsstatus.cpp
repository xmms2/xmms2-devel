#include <qpainter.h>
#include <qwidget.h>

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-qt.h>
#include "xmmsstatus.h"


XMMSStatus::XMMSStatus (XMMSClientQT *client, QWidget *parent) :
			QWidget (parent, "XMMSStatus")
{

	m_client = client;
	setBackgroundColor( white );

	m_str = new QString ("Musik som fan apan");
	m_tme = new QString ("00:30");
}

void
XMMSStatus::paintEvent (QPaintEvent *event)
{

	QPainter p (this);
	int i;
	p.setPen (Qt::black);

	p.setWindow (0, 0, 100, 50);

/*	for (i=0; i < FFT_LEN; i++) {

		float sum = 0.0f;
		int j;

		for (j = 0; j < 32; j++) {
			sum += spec
		
	}*/

/*	QFont f ("fixed", 10, QFont::Light);
	f.setStretch (QFont::ExtraCondensed);
	
	p.drawRect (1, 1, 99, 46);

	p.setFont (f);
	QFontMetrics fm = p.fontMetrics();
	int y = 10 + fm.ascent();
	p.drawText (10, y, *m_str);*/

	
	

}

QSize
XMMSStatus::sizeHint () const
{
	qDebug ("Hejhopp");
	return QSize (100,100);
}

