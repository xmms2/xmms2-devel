#ifndef __XMMS_PLUGIN_H__
#define __XMMS_PLUGIN_H__

#include <glib.h>
#include <gmodule.h>

/* 
 * Plugin methods
 */

#define XMMS_METHOD_CAN_HANDLE "can_handle"
#define XMMS_METHOD_OPEN "open"
#define XMMS_METHOD_CLOSE "close"
#define XMMS_METHOD_READ "read"
#define XMMS_METHOD_SIZE "size"
#define XMMS_METHOD_SEEK "seek"
#define XMMS_METHOD_NEW "new"
#define XMMS_METHOD_DECODE_BLOCK "decode_block"
#define XMMS_METHOD_GET_MEDIAINFO "get_mediainfo"
#define XMMS_METHOD_DESTROY "destroy"
#define XMMS_METHOD_WRITE "write"
#define XMMS_METHOD_SEARCH "search"

/*
 * Plugin properties.
 */

/* For transports */
#define XMMS_PLUGIN_PROPERTY_SEEK (1 << 0)
#define XMMS_PLUGIN_PROPERTY_LOCAL (1 << 1)

/* For decoders */
#define XMMS_PLUGIN_PROPERTY_FAST_FWD (1 << 2)
#define XMMS_PLUGIN_PROPERTY_REWIND (1 << 3)
#define XMMS_PLUGIN_PROPERTY_SUBTUNES (1 << 4)

/* For output */

/*
 * Type declarations
 */

typedef enum {
	XMMS_PLUGIN_TYPE_TRANSPORT,
	XMMS_PLUGIN_TYPE_DECODER,
	XMMS_PLUGIN_TYPE_OUTPUT,
	XMMS_PLUGIN_TYPE_MEDIALIB
} xmms_plugin_type_t;

typedef struct {
	GMutex *mtx;
	GModule *module;
	GList *info_list;
	xmms_plugin_type_t type;
	gchar *name;
	gchar *shortname;
	gchar *description;
	gint properties;

	guint users;
	GHashTable *method_table;
} xmms_plugin_t;

typedef struct {
	gchar *key;
	gchar *value;
} xmms_plugin_info_t;

typedef void *xmms_plugin_method_t;

/*
 * Public functions
 */

xmms_plugin_t *xmms_plugin_new (xmms_plugin_type_t type, 
					const gchar *shortname,
					const gchar *name,
					const gchar *description);
void xmms_plugin_method_add (xmms_plugin_t *plugin, const gchar *name,
							 xmms_plugin_method_t method);

xmms_plugin_type_t xmms_plugin_type_get (const xmms_plugin_t *plugin);
const char *xmms_plugin_name_get (const xmms_plugin_t *plugin);
const gchar *xmms_plugin_shortname_get (const xmms_plugin_t *plugin);
const char *xmms_plugin_description_get (const xmms_plugin_t *plugin);
void xmms_plugin_properties_add (xmms_plugin_t* const plugin, gint property);
void xmms_plugin_properties_remove (xmms_plugin_t* const plugin, gint property);
gboolean xmms_plugin_properties_check (const xmms_plugin_t *plugin, gint property);
void xmms_plugin_info_add (xmms_plugin_t *plugin, gchar *key, gchar *value);
const GList *xmms_plugin_info_get (const xmms_plugin_t *plugin);

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
