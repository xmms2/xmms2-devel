#ifndef _XMMS_OUTPUT_H_
#define _XMMS_OUTPUT_H_

#include "object.h"
#include "plugin.h"
#include "ringbuf.h"

#include <glib.h>

typedef struct xmms_output_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;

	GMutex *mutex;
	GThread *thread;
	gboolean running;

	xmms_ringbuf_t *buffer;
	
	gpointer plugin_data;
} xmms_output_t;

gpointer xmms_output_plugin_data_get (xmms_output_t *output);
void xmms_output_plugin_data_set (xmms_output_t *output, gpointer data);
xmms_output_t * xmms_output_open (xmms_plugin_t *plugin);
void xmms_output_start (xmms_output_t *output);
void xmms_output_write (xmms_output_t *output, gpointer buffer, gint len);
gint xmms_output_read (xmms_output_t *output, gchar *buffer, gint len);
xmms_plugin_t * xmms_output_find_plugin ();

typedef void (*xmms_output_write_method_t) (xmms_output_t *output, gchar *buffer, gint len);
typedef gboolean (*xmms_output_open_method_t) (xmms_output_t *output, const gchar *path);

#endif
