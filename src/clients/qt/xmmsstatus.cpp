#include <stdlib.h>
#include <qpainter.h>
#include <qwidget.h>

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-qt.h>
#include "xmmsstatus.h"

#define FFT_BITS    10                                                                                                                                                
#define FFT_LEN     (1<<FFT_BITS)                                                                                                                                     


XMMSStatus::XMMSStatus (XMMSClientQT *client, QWidget *parent) :
			QWidget (parent, "XMMSStatus")
{

	m_client = client;
//	setBackgroundColor( white );
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

	for (i=0; i < FFT_BITS; i++) {
		int h = (int)((m_vis_data[i]/255.0)*30.0);
		p.fillRect (10+i*29, 30-h, 15, h, QBrush(Qt::black, Qt::SolidPattern));
	}
	free (m_vis_data);
	m_vis_data = NULL;
}

QSize
XMMSStatus::sizeHint () const
{
	return QSize (300,30);
}

