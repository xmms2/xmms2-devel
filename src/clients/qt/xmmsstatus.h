/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *                   
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */




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
