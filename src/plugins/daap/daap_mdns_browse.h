#ifndef DAAP_MDNS_BROWSE_H
#define DAAP_MDNS_BROWSE_H

#include <glib.h>

typedef struct {
	gchar *server_name;
	gchar *address;
	gchar *mdns_hostname;
	guint16 port;
} daap_mdns_server_t;

gboolean daap_mdns_initialize();
GSList * daap_mdns_get_server_list();

#endif
