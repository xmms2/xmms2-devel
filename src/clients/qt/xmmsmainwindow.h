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




#ifndef __XMMSMAINWINDOW_H__
#define __XMMSMAINWINDOW_H__

#include <qapplication.h>
#include <qmainwindow.h>
#include <qstatusbar.h>
#include <xmms/xmmsclient-qt.h>

#include "xmmslistview.h"
#include "xmmstoolbar.h"

class XMMSMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	XMMSMainWindow (XMMSClientQT *);
	void setCurrentId (guint);
	void setInfo (GHashTable *);
	void getInfo (guint);
	void add (unsigned int);
	void clear ();

protected:
	XMMSListView *m_listview;
	XMMSToolbar *m_toolbar;
	//XMMSStatusBar *m_statusbar;
	
	XMMSClientQT *m_client;
};

#endif

