#ifndef __XMMS_TRANSPORT_H__
#define __XMMS_TRANSPORT_H__

#include <glib.h>

#include "xmms/plugin.h"

/*
 * Type definitions
 */

typedef struct {
	xmms_plugin_t *plugin;
	gchar *mime_type;
	gpointer plugin_data;
} xmms_transport_t;

/*
 * Transport plugin methods
 */

typedef gboolean (*xmms_transport_can_handle_method_t) (const gchar *uri);
typedef gchar * (*xmms_transport_open_method_t) (const gchar *uri);
typedef void (*xmms_transport_close_method_t) (xmms_transport_t *transport);

/*
 * Function prototypes
 */

xmms_transport_t *xmms_transport_open (const gchar *uri);
void xmms_transport_close (xmms_transport_t *transport);

const gchar *xmms_transport_mime_type_get (xmms_transport_t *transport);

#endif /* __XMMS_TRANSPORT_H__ */
