#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>

/*
 * Type definitions
 */

typedef struct {
	FILE *fp;
} xmms_diskwrite_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_diskwrite_open (xmms_output_t *output, const gchar *path);
void xmms_diskwrite_write (xmms_output_t *output, gchar *buffer, gint len);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "Diskwriter output " VERSION,
							  "Saves a wavefile");
	
	xmms_plugin_method_add (plugin, "write", xmms_diskwrite_write);
	xmms_plugin_method_add (plugin, "open", xmms_diskwrite_open);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_diskwrite_open (xmms_output_t *output, const gchar *path)
{
	FILE *fp;
	xmms_diskwrite_data_t *data;

	XMMS_DBG ("xmms_diskwrite_open (%p, %s)", output, path);
	
	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (path, FALSE);

	XMMS_DBG ("Opening %s", path);
	fp = fopen (path, "rb");
	XMMS_DBG ("fp: %p", fp);
	if (!fp)
		return FALSE;

	data = g_new0 (xmms_diskwrite_data_t, 1);
	data->fp = fp;

	xmms_output_plugin_data_set (output, data);
	
	return TRUE;
}

void
xmms_diskwrite_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_diskwrite_data_t *data;
	
	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	XMMS_DBG ("diskwriter writes %d to %p", len, data->fp);

	fwrite (buffer, 1, len, data->fp);
	
}
