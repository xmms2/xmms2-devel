#ifndef __XMMS_WATCH_H__
#define __XMMS_WATCH_H__

#include "xmmsclient.h"
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	XMMSC_WATCH_IN = 1 << 0,
	XMMSC_WATCH_OUT = 1 << 1,
	XMMSC_WATCH_ERROR = 1 << 2,
	XMMSC_WATCH_HANGUP = 1 << 3,
} xmmsc_watch_flags_t;

typedef enum {
	XMMSC_WATCH_ADD,
	XMMSC_WATCH_REMOVE,
	XMMSC_TIMEOUT_ADD,
	XMMSC_TIMEOUT_REMOVE,
	XMMSC_WATCH_WAKEUP
} xmmsc_watch_action_t;

typedef int (*xmmsc_watch_callback_t) (xmmsc_connection_t *conn, 
				       xmmsc_watch_action_t action,
				       void *data);
typedef gboolean (*xmmsc_timeout_callback_t) (gpointer data);

typedef struct xmmsc_watch_St {
	DBusWatch *dbus_watch;
	gpointer data;
	int fd;
	unsigned int flags;
} xmmsc_watch_t;

typedef struct xmmsc_timeout_St {
	DBusTimeout *dbus_timeout;
	int interval; /* in milliseconds */
	xmmsc_timeout_callback_t cb;
	void *data;
} xmmsc_timeout_t;

void xmmsc_watch_init (xmmsc_connection_t *connection);
void xmmsc_watch_callback_set (xmmsc_connection_t *conn, xmmsc_watch_callback_t cb);
gpointer xmmsc_watch_data_get (xmmsc_connection_t *connection);
void xmmsc_watch_data_set (xmmsc_connection_t *connection, gpointer data);
gboolean xmmsc_watch_more (xmmsc_connection_t *conn);
void xmmsc_watch_dispatch (xmmsc_connection_t *conn, xmmsc_watch_t *watch, unsigned int event);

#ifdef __cplusplus
}
#endif

#endif
