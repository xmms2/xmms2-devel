#ifndef _XMMS_OUTPUT_H_
#define _XMMS_OUTPUT_H_

#include <glib.h>

/*
 * Type definitions
 */

typedef struct xmms_output_St xmms_output_t;


/*
 * Output plugin methods
 */

typedef void (*xmms_output_write_method_t) (xmms_output_t *output, gchar *buffer, gint len);
typedef gboolean (*xmms_output_open_method_t) (xmms_output_t *output);
typedef void (*xmms_output_close_method_t) (xmms_output_t *output);
typedef guint (*xmms_output_samplerate_set_method_t) (xmms_output_t *output, guint rate);

/*
 * Public function prototypes
 */

gpointer xmms_output_plugin_data_get (xmms_output_t *output);
void xmms_output_plugin_data_set (xmms_output_t *output, gpointer data);

gchar *xmms_output_config_string_get (xmms_output_t *output, gchar *val);


#endif
