#ifndef __XMMS_TRANSPORT_H__
#define __XMMS_TRANSPORT_H__

#include <glib.h>

/*
 * Type definitions
 */

typedef struct xmms_transport_St xmms_transport_t;

/*
 * Transport plugin methods
 */

typedef gboolean (*xmms_transport_can_handle_method_t) (const gchar *uri);
typedef gboolean (*xmms_transport_open_method_t) (xmms_transport_t *transport,
												  const gchar *uri);
typedef void (*xmms_transport_close_method_t) (xmms_transport_t *transport);
typedef gint (*xmms_transport_read_method_t) (xmms_transport_t *transport,
											  gchar *buffer, guint length);

typedef gboolean (*xmms_transport_eof_method_t) (xmms_transport_t *transport);

/*
 * Public function prototypes
 */

gpointer xmms_transport_plugin_data_get (xmms_transport_t *transport);
void xmms_transport_plugin_data_set (xmms_transport_t *transport, gpointer data);

void xmms_transport_mime_type_set (xmms_transport_t *transport, const gchar *mimetype);

gint xmms_transport_read (xmms_transport_t *transport, gchar *buffer, guint len);
void xmms_transport_wait (xmms_transport_t *transport);
 
/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_transport_t *xmms_transport_open (const gchar *uri);
void xmms_transport_close (xmms_transport_t *transport);

const gchar *xmms_transport_mime_type_get (xmms_transport_t *transport);

void xmms_transport_start (xmms_transport_t *transport);

#endif /* __XMMS_TRANSPORT_H__ */
