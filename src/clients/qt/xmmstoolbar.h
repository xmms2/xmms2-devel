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

