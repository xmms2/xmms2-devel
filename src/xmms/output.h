#ifndef _XMMS_OUTPUT_H_
#define _XMMS_OUTPUT_H_

#include "object.h"
#include "plugin.h"
#include "ringbuf.h"
#include "config_xmms.h"

#include <glib.h>

/*
 * Type definitions
 */

typedef struct xmms_output_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;

	GMutex *mutex;
	GCond *cond;
	GThread *thread;
	gboolean running;

	xmms_ringbuf_t *buffer;
	xmms_config_value_t *config;
	
	gpointer plugin_data;
} xmms_output_t;

/*
 * Output plugin methods
 */

typedef void (*xmms_output_write_method_t) (xmms_output_t *output, gchar *buffer, gint len);
typedef gboolean (*xmms_output_open_method_t) (xmms_output_t *output, const gchar *path);

/*
 * Public function prototypes
 */

gpointer xmms_output_plugin_data_get (xmms_output_t *output);
void xmms_output_plugin_data_set (xmms_output_t *output, gpointer data);

void xmms_output_write (xmms_output_t *output, gpointer buffer, gint len);
gchar * xmms_output_get_config_string (xmms_output_t *output, gchar *val);


/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_output_t * xmms_output_open (xmms_plugin_t *plugin, xmms_config_data_t *config);
void xmms_output_start (xmms_output_t *output);
void xmms_output_set_eos (xmms_output_t *output, gboolean eos);
xmms_plugin_t * xmms_output_find_plugin ();

#endif
