#ifndef __XMMS_MEDIALIB_H__
#define __XMMS_MEDIALIB_H__

#include "xmms/plugin.h"
#include "xmms/playlist.h"
#include <glib.h>

typedef struct xmms_medialib_St {
	GMutex *mutex;
	
	xmms_plugin_t *plugin;

	/** Private data */
	gpointer data;
} xmms_medialib_t;

/*
 * Method defintions.
 */ 

typedef gboolean (*xmms_medialib_new_method_t) (xmms_medialib_t *medialib);
typedef GList *(*xmms_medialib_search_method_t) (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry);
typedef void (*xmms_medialib_add_entry_method_t) (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry);
typedef void (*xmms_medialib_close_method_t) (xmms_medialib_t *medialib);

/*
 * Public interface
 */

xmms_plugin_t *xmms_medialib_find_plugin (gchar *name);
xmms_medialib_t *xmms_medialib_init (xmms_plugin_t *plugin);
void xmms_medialib_close (xmms_medialib_t *medialib);

void xmms_medialib_set_data (xmms_medialib_t *medialib, gpointer data);
gpointer xmms_medialib_get_data (xmms_medialib_t *medialib);
GList *xmms_medialib_search (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry);
void xmms_medialib_list_free (GList *list);
void xmms_medialib_add_entry (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry);
void xmms_medialib_add_dir (xmms_medialib_t *medialib, const gchar *dir);
gboolean xmms_medialib_check_if_exists (xmms_medialib_t *medialib, gchar *uri);


#endif /* __XMMS_MEDIALIB_H__ */
