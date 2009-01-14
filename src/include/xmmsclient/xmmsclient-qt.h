/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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




#ifndef __XMMSCLIENT_QT_H__
#define __XMMSCLIENT_QT_H__

#include <qsocketnotifier.h>
#include <qobject.h>
#include <qlist.h>

#include "xmms/xmmsclient.h"
#include "xmms/xmmswatch.h"

class XMMSClientWatch : public QObject {
	Q_OBJECT

public:
	XMMSClientWatch (xmmsc_connection_t *conn, xmmsc_watch_t *watch, QObject *parent);
	~XMMSClientWatch ();

protected slots:
	void onRead ();
	void onWrite ();
	void onException ();

protected:
	xmmsc_connection_t *m_conn;
	xmmsc_watch_t *m_watch;

	QSocketNotifier *m_notifier_w;
	QSocketNotifier *m_notifier_r;
	QSocketNotifier *m_notifier_e;
};

class XMMSClientQT : public QObject {
	Q_OBJECT

public:
	XMMSClientQT (xmmsc_connection_t *, QObject *);
	xmmsc_connection_t * XMMSClientQT::getConnection ();

	void watch_add (xmmsc_watch_t *);
	void watch_remove (xmmsc_watch_t *);
	void wakeUp ();
	void timeout_add (xmmsc_timeout_t *);
	void timeout_remove (xmmsc_timeout_t *);
	
protected:
	QPtrList<XMMSClientWatch> m_watch_list;
	xmmsc_connection_t *m_conn;
};

#endif
