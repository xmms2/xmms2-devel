#ifndef __XMMS_PLAYLIST_PLUGIN_H__
#define __XMMS_PLAYLIST_PLUGIN_H__


#include "xmms/transport.h"
#include "xmms/playlist.h"

/* Type definitions */

typedef struct xmms_playlist_plugin_St {
	xmms_plugin_t *plugin;
} xmms_playlist_plugin_t;

/*
 * Plugin methods.
 */

typedef gboolean (*xmms_playlist_plugin_read_method_t) (xmms_transport_t *transport, xmms_playlist_t *playlist);
typedef gboolean (*xmms_playlist_plugin_can_handle_method_t) (const gchar *mimetype);
typedef gboolean (*xmms_playlist_plugin_write_method_t) (xmms_playlist_t *playlist, gchar *filename);

/*
 * Public functions.
 */

xmms_playlist_plugin_t *xmms_playlist_plugin_new (const gchar *mimetype);
void xmms_playlist_plugin_free (xmms_playlist_plugin_t *plsplugin);
void xmms_playlist_plugin_read (xmms_playlist_plugin_t *plsplugin, xmms_playlist_t *playlist, xmms_transport_t *transport);
gboolean xmms_playlist_plugin_save (xmms_playlist_plugin_t *plsplugin, xmms_playlist_t *playlist, gchar *filename);


#endif
