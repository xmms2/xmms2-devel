#ifndef _XMMS_OUTPUT_INT_H_
#define _XMMS_OUTPUT_INT_H_

#include "xmms/plugin.h"
#include "xmms/config_xmms.h"

/*
 * Private function prototypes -- do NOT use in plugins.
 */

xmms_output_t * xmms_output_new (xmms_plugin_t *plugin, xmms_config_data_t *config);
gboolean xmms_output_open (xmms_output_t *output);
void xmms_output_close (xmms_output_t *output);
void xmms_output_start (xmms_output_t *output);
void xmms_output_set_eos (xmms_output_t *output, gboolean eos);
xmms_plugin_t * xmms_output_find_plugin ();
void xmms_output_write (xmms_output_t *output, gpointer buffer, gint len);
guint xmms_output_samplerate_set (xmms_output_t *output, guint rate);
void xmms_output_played_samples_set (xmms_output_t *output, guint samples);

#endif
