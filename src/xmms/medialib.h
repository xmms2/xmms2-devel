#ifndef __XMMS_MEDIALIB_H__
#define __XMMS_MEDIALIB_H__

#include "plugin.h"
#include <glib.h>

typedef struct xmms_medialib_St {
	GMutex *mutex;
	
	xmms_plugin_t *plugin;

	/** Private data */
	gpointer data;
} xmms_medialib_t;

typedef gboolean (*xmms_medialib_new_method_t) (xmms_medialib_t *medialib);

xmms_plugin_t *xmms_medialib_find_plugin (gchar *name);
xmms_medialib_t *xmms_medialib_init (xmms_plugin_t *plugin);

void xmms_medialib_set_data (xmms_medialib_t *medialib, gpointer data);
gpointer xmms_medialib_get_data (xmms_medialib_t *medialib);


#endif /* __XMMS_MEDIALIB_H__ */
