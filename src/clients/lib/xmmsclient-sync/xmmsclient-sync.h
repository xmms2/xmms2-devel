#ifndef __XMMS_CLIENT_SYNC_H__
#define __XMMS_CLIENT_SYNC_H__

#include <glib.h>
#include "xmmsclient.h"

#ifdef __cplusplus
extern "C" {
#endif

void xmmsc_sync_init (xmmsc_connection_t *);
gint xmmsc_sync_played_time_get ();

#ifdef __cplusplus
}
#endif

#endif
