#ifndef __XMMS_PLAYLIST_PLUGIN_H__
#define __XMMS_PLAYLIST_PLUGIN_H__


#include "transport.h"
#include "playlist.h"

/* Type definitions */

typedef struct xmms_playlist_plugin_St {
	xmms_plugin_t *plugin;
} xmms_playlist_plugin_t;

/*
 * Plugin methods.
 */

typedef gboolean (*xmms_playlist_plugin_read_method) (xmms_transport_t *transport, xmms_playlist_t *playlist);
typedef gboolean (*xmms_playlist_plugin_can_handle_method) (const gchar *mimetype);
typedef gchar *(*xmms_playlist_plugin_write_method) (xmms_playlist_t *playlist, gint *size);

/*
 * Public functions.
 */

xmms_playlist_plugin_t *xmms_playlist_plugin_new (const gchar *mimetype);
void xmms_playlist_plugin_free (xmms_playlist_plugin_t *plsplugin);
void xmms_playlist_plugin_read (xmms_playlist_plugin_t *plsplugin, xmms_playlist_t *playlist, xmms_transport_t *transport);


#endif
