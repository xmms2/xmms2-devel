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
#include <qprogressbar.h>
#include <qmainwindow.h>
#include <qstatusbar.h>
#include <qlabel.h>
#include <xmms/xmmsclient-qt.h>
#include <internal/xhash-int.h>
#include <internal/xlist-int.h>

#include "xmmslistview.h"
#include "xmmstoolbar.h"

class XMMSMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	XMMSMainWindow (XMMSClientQT *);
	void setCurrentId (int);
	void setInfo (x_hash_t *);
	void getInfo (int);
	void XMMSMainWindow::add (int id);
	void add (unsigned int);
	void clear ();
	XMMSClientQT *client (void) { return m_client; };
	void setID (int id) { m_id = id; };
	int ID (void) { return m_id; };
	XMMSToolbar *toolbar (void) { return m_toolbar; };
	XMMSListView *pl (void) { return m_listview; };
	x_hash_t *idHash (void) { return m_idhash; };
	XMMSListViewItem *cItem (void) { return m_citem; };
	void *setcItem (XMMSListViewItem *i) { m_citem = i; };
	QProgressBar *bar (void) { return m_bar; };
	QLabel *bartxt (void) { return m_bartxt; };
	bool barBusy (void) { return m_barbusy; };
	void setBarBusy (bool m) { m_barbusy = m; };
	x_list_t *getVisTime (void) { return m_vis_time; };
	x_list_t *getVisData (void) { return m_vis_data; };
	void setVisTime (x_list_t *vis_time) { m_vis_time = vis_time; };
	void setVisData (x_list_t *vis_data) { m_vis_data = vis_data; };
protected:
	XMMSListView *m_listview;
	XMMSToolbar *m_toolbar;
	QProgressBar *m_bar;
	QLabel *m_bartxt;

	XMMSListViewItem *m_citem;
	
	XMMSClientQT *m_client;
	int m_id;
	x_hash_t *m_idhash;
	bool m_barbusy;
	x_list_t *m_vis_data;
	x_list_t *m_vis_time;
};

#endif

