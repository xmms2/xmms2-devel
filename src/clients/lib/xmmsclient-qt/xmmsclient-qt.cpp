
#include <qptrlist.h>

#include <qsocketnotifier.h>
#include <qobject.h>

#include "xmmswatch.h"
#include "xmmsclient.h"

#include "xmmsclient-qt.h"

static int
watch_callback (xmmsc_connection_t *conn,
		xmmsc_watch_action_t action,
		void *data)
{
	XMMSClientQT *cqt;

	cqt = (XMMSClientQT *) xmmsc_watch_data_get (conn);

	switch (action) {
		case XMMSC_WATCH_ADD:
			{
				xmmsc_watch_t *watch = (xmmsc_watch_t *) data;
				cqt->watch_add (watch);
			}
			break;
		case XMMSC_WATCH_REMOVE:
			{
				xmmsc_watch_t *watch = (xmmsc_watch_t *) data;
				cqt->watch_remove (watch);
			}
			break;
		case XMMSC_TIMEOUT_ADD:
			{
				xmmsc_timeout_t *timeout = (xmmsc_timeout_t *) data;
				cqt->timeout_add (timeout);
			}
			break;
		case XMMSC_TIMEOUT_REMOVE:
			{
				xmmsc_timeout_t *timeout = (xmmsc_timeout_t *) data;
				cqt->timeout_remove (timeout);
			}
			break;
		case XMMSC_WATCH_WAKEUP:
			cqt->wakeUp ();
			break;
	}

	return 1;
}

/**
 * @internal
 */

XMMSClientWatch::XMMSClientWatch (xmmsc_connection_t *conn, 
				  xmmsc_watch_t *watch, 
				  QObject *parent) :
		 QObject (parent, NULL)
{
	m_watch = watch;
	m_conn = conn;

	if (watch->flags & XMMSC_WATCH_IN) {
		m_notifier_r = new QSocketNotifier (watch->fd, QSocketNotifier::Read, this);
		connect (m_notifier_r, SIGNAL (activated (int)), this, SLOT (onRead ()));
		m_notifier_r->setEnabled (TRUE);
	}
	if (watch->flags & XMMSC_WATCH_OUT) {
		m_notifier_w = new QSocketNotifier (watch->fd, QSocketNotifier::Write, this);
		connect (m_notifier_w, SIGNAL (activated (int)), this, SLOT (onWrite ()));
		m_notifier_w->setEnabled (TRUE);
	}

//	m_notifier_e = new QSocketNotifier (watch->fd, QSocketNotifier::Exception, this);
//	connect (m_notifier_e, SIGNAL (activated (int)), this, SLOT (onException ()));
//	m_notifier_e->setEnabled (TRUE);

}

XMMSClientWatch::~XMMSClientWatch ()
{
}

void
XMMSClientWatch::onRead ()
{
	unsigned int flags = 0;

	flags |= XMMSC_WATCH_IN;

	qDebug ("Dispatch Read");

	xmmsc_watch_dispatch (m_conn, m_watch, flags);
}

void
XMMSClientWatch::onWrite ()
{
	unsigned int flags = 0;

	flags |= XMMSC_WATCH_OUT;
	qDebug ("Dispatch Write");

	xmmsc_watch_dispatch (m_conn, m_watch, flags);
}

void
XMMSClientWatch::onException ()
{
	unsigned int flags = 0;

	flags |= XMMSC_WATCH_HANGUP;

	xmmsc_watch_dispatch (m_conn, m_watch, flags);
}

/**
 * @defgroup XMMSWatchQT XMMSWatchQT
 * @brief QT Watch bindnings.
 * 
 * This functions should be used in a QT client to get the 
 * Messages from the server.
 *
 * @ingroup XMMSWatch
 * @{
 */

/**
 * This class should be inited after xmmsc_connect() is called.
 * When this is inited you don't have to bother about it anymore.
 * Callbacks will still be set by xmmsc_set_callback()
 *
 * @sa xmmsc_connect
 * @sa xmmsc_set_callback
 */

XMMSClientQT::XMMSClientQT (xmmsc_connection_t *conn, QObject *parent) : 
	      QObject (parent)
{
	m_conn = conn;
	xmmsc_watch_callback_set (conn, watch_callback);
	xmmsc_watch_data_set (conn, this);
	xmmsc_watch_init (conn);
}

/** @} */

void
XMMSClientQT::watch_add (xmmsc_watch_t *watch)
{
	XMMSClientWatch *q_watch = new XMMSClientWatch (m_conn, watch, this);
	watch->data = q_watch;
	qDebug ("Adding watch for %d", watch->fd);
	m_watch_list.append (q_watch);
}

void
XMMSClientQT::watch_remove (xmmsc_watch_t *watch)
{
	XMMSClientWatch *w = (XMMSClientWatch *)watch->data;
	qDebug ("Removing watch for %d", watch->fd);
	m_watch_list.remove (w);

	delete w;
}

void
XMMSClientQT::timeout_add (xmmsc_timeout_t *timeout)
{
	qDebug ("Want a timeout");
}

void
XMMSClientQT::timeout_remove (xmmsc_timeout_t *timeout)
{
	qDebug ("Want to remove timeout");
}

void
XMMSClientQT::wakeUp ()
{
}
