#include <stdlib.h>
#include <qpainter.h>
#include <qwidget.h>
#include <math.h>

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

	for (i=0; i<FFT_LEN/32/2; i++) {
		float sum = 0.0f;
		int s;
		int j;
		for (j=0; j < 32; j++) {
			sum += m_vis_data[i*16+j];
		}
		if (sum != 0) {
			sum = log (sum/32);
		}
		sum = sum*64;
		s = (int)((sum / 255.0) * 30.0);
		p.fillRect (i*16, 30-s, 15, s, QBrush(Qt::black, Qt::SolidPattern));
	}

	free (m_vis_data);
	m_vis_data = NULL;
}

QSize
XMMSStatus::sizeHint () const
{
	return QSize (300,30);
}

