
#ifndef XMMS_SOCKET_UNIX_H
#define XMMS_SOCKET_UNIX_H

#include "xmms/ipc_transport.h"

xmms_ipc_transport_t *xmms_ipc_usocket_server_init (gchar *path);
xmms_ipc_transport_t *xmms_ipc_usocket_client_init (gchar *path);

#endif /* XMMS_SOCKET_UNIX_H */
