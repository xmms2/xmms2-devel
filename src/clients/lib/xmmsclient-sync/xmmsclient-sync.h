#ifndef __XMMS_CLIENT_SYNC_H__
#define __XMMS_CLIENT_SYNC_H__

#include <glib.h>
#include "xmmsclient.h"

#ifdef __cplusplus
extern "C" {
#endif

void xmmsc_sync_init (xmmsc_connection_t *);
gint xmmsc_sync_played_time_get ();
gint xmmsc_sync_current_id_get ();
GHashTable * xmmsc_sync_entry_get (gint id);

#ifdef __cplusplus
}
#endif

#endif
