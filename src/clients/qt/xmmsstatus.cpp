#include <stdlib.h>
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
	setSizePolicy (QSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed, FALSE));

	m_str = new QString ("XMMS2 - X Music Multiplexer System");
	m_tme = new QString ("00:00");
	m_ctme = new QString ("00:00");
}

void 
XMMSStatus::setVisData (float *data) 
{
	m_vis_data = data;
	repaint();
}

void
XMMSStatus::setDisplayText (QString *str)
{
	m_str = str;
}

void
XMMSStatus::setTME (QString *str)
{
	m_tme = str;
}

void
XMMSStatus::setCTME (QString *str)
{
	m_ctme = str;
}

void
XMMSStatus::paintEvent (QPaintEvent *event)
{

	QPainter p (this);
	int i;

	if (!m_vis_data)
		return;
	
	p.setPen (Qt::black);

	p.setWindow (0, 0, 300, 30);

//	QFont f ("fixed", 6);
	
	p.drawRect (0, 0, 300, 30);

//	p.setFont (f);
//	QFontMetrics fm = p.fontMetrics();
//	int y = 2 + fm.ascent();

	/* Write song title */
//	p.drawText (2, y, *m_str);
//	p.drawText (300-fm.width (*m_tme)-1, y+fm.ascent(), *m_tme);
//	p.drawText (300-fm.width (*m_tme)-fm.width(*m_ctme+"/"), y+fm.ascent(), *m_ctme+"/");

	for (i=0; i < 10; i++) {
		printf ("| %f ", m_vis_data[i]);
	}
	printf ("\n");
	free (m_vis_data);
	m_vis_data = NULL;
}

QSize
XMMSStatus::sizeHint () const
{
	return QSize (300,30);
}

