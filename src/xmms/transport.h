#ifndef __XMMS_TRANSPORT_H__
#define __XMMS_TRANSPORT_H__

#include <glib.h>
#include "xmms/plugin.h"
#include "xmms/playlist.h"

/*
 * Type definitions
 */

typedef struct xmms_transport_St xmms_transport_t;

#define XMMS_TRANSPORT_SEEK_SET 0
#define XMMS_TRANSPORT_SEEK_END 1
#define XMMS_TRANSPORT_SEEK_CUR 2

/*
 * Transport plugin methods
 */

typedef gboolean (*xmms_transport_can_handle_method_t) (const gchar *uri);
typedef gboolean (*xmms_transport_open_method_t) (xmms_transport_t *transport,
												  const gchar *uri);
typedef void (*xmms_transport_close_method_t) (xmms_transport_t *transport);
typedef gint (*xmms_transport_read_method_t) (xmms_transport_t *transport,
											  gchar *buffer, guint length);

typedef gint (*xmms_transport_seek_method_t) (xmms_transport_t *transport, guint offset, gint whence);
typedef gint (*xmms_transport_size_method_t) (xmms_transport_t *transport);

/*
 * Public function prototypes
 */

gpointer xmms_transport_plugin_data_get (xmms_transport_t *transport);
void xmms_transport_plugin_data_set (xmms_transport_t *transport, gpointer data);

void xmms_transport_mime_type_set (xmms_transport_t *transport, const gchar *mimetype);

gint xmms_transport_read (xmms_transport_t *transport, gchar *buffer, guint len);
void xmms_transport_wait (xmms_transport_t *transport);
void xmms_transport_seek (xmms_transport_t *transport, gint offset, gint whence);
gint xmms_transport_size (xmms_transport_t *transport);
xmms_plugin_t *xmms_transport_get_plugin (const xmms_transport_t *transport);

const gchar *xmms_transport_uri_get(const xmms_transport_t *const transport);
xmms_playlist_entry_t * xmms_transport_entry_get (xmms_transport_t *transport);
const gchar *xmms_transport_suburi_get(const xmms_transport_t *const transport);

xmms_transport_t *xmms_transport_open_plugin (xmms_plugin_t *plugin, 
		xmms_playlist_entry_t *entry, gpointer data);

void xmms_transport_close (xmms_transport_t *transport);

gboolean xmms_transport_is_local (xmms_transport_t *transport);

#endif /* __XMMS_TRANSPORT_H__ */
