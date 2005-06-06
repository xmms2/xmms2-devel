#ifndef __MLIB_UPDATER_H__
#define __MLIB_UPDATER_H__

#include <glib.h>
#include <xmmsclient/xmmsclient.h>

#define DBG(fmt, args...) g_message(fmt, ## args)
#define ERR(fmt, args...) g_warning(fmt, ## args)
#define VERSION "0.1-WIP"

typedef struct xmontior_St {
	gpointer data;
	xmmsc_connection_t *conn;
	gchar *watch_dir;
	GList *dir_list;
} xmonitor_t;

typedef enum {
	MON_DIR_CHANGED,
	MON_DIR_CREATED,
	MON_DIR_DELETED,
	MON_DIR_MOVED
} mon_event_code_t;

#define MON_FILENAME_MAX PATH_MAX

typedef struct xevent_St {
	gchar filename[MON_FILENAME_MAX];
	mon_event_code_t code;
} xevent_t;

gint monitor_init (xmonitor_t *mon);
gboolean monitor_add_dir (xmonitor_t *mon, gchar *dir);
gboolean monitor_process (xmonitor_t *mon, xevent_t *event);
gboolean monitor_del_dir (xmonitor_t *mon, gchar *dir);

#endif
