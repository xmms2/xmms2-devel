#ifndef __XMMS_PLUGIN_H__
#define __XMMS_PLUGIN_H__

#include <glib.h>
#include <gmodule.h>

/*
 * Type declarations
 */

typedef enum {
	XMMS_PLUGIN_TYPE_TRANSPORT,
	XMMS_PLUGIN_TYPE_DECODER,
	XMMS_PLUGIN_TYPE_OUTPUT,
	XMMS_PLUGIN_TYPE_MEDIAINFO
} xmms_plugin_type_t;

typedef struct {
	GMutex *mtx;
	GModule *module;
	xmms_plugin_type_t type;
	char *name;
	char *description;

	guint users;
	GHashTable *method_table;
} xmms_plugin_t;

typedef void *xmms_plugin_method_t;

/*
 * Public functions
 */

xmms_plugin_t *xmms_plugin_new (xmms_plugin_type_t type, const char *name,
								const char *description);
void xmms_plugin_method_add (xmms_plugin_t *plugin, const gchar *name,
							 xmms_plugin_method_t method);

xmms_plugin_type_t xmms_plugin_type_get (const xmms_plugin_t *plugin);
const char *xmms_plugin_name_get (const xmms_plugin_t *plugin);
const char *xmms_plugin_description_get (const xmms_plugin_t *plugin);

/*
 * Private functions
 */

gboolean xmms_plugin_init (void);
void xmms_plugin_scan_directory (const gchar *dir);

void xmms_plugin_ref (xmms_plugin_t *plugin);
void xmms_plugin_unref (xmms_plugin_t *plugin);

GList *xmms_plugin_list_get (xmms_plugin_type_t type);
void xmms_plugin_list_destroy (GList *list);

xmms_plugin_method_t xmms_plugin_method_get (xmms_plugin_t *plugin,
											 const gchar *member);

#endif /* __XMMS_PLUGIN_H__ */
