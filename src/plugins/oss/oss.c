#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/ringbuf.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>


#include <glib.h>

/*
 * Type definitions
 */

typedef struct xmms_oss_data_St {
	gint fd;
} xmms_oss_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_oss_open (xmms_output_t *output);
static void xmms_oss_close (xmms_output_t *output);
static void xmms_oss_write (xmms_output_t *output, gchar *buffer, gint len);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "oss",
			"OSS Output " VERSION,
			"OpenSoundSystem output plugin");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, xmms_oss_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, xmms_oss_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_oss_close);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_oss_open (xmms_output_t *output)
{
	xmms_oss_data_t *data;
	gchar *dev;
	guint param;

	XMMS_DBG ("xmms_oss_open (%p)", output);
	
	g_return_val_if_fail (output, FALSE);

	dev = xmms_output_get_config_string (output, "device");
	if (!dev) {
		XMMS_DBG ("device not found in config, using default");
		dev = "/dev/dsp";
	}

	XMMS_DBG ("device = %s", dev);

	data = g_new0 (xmms_oss_data_t, 1);
	data->fd = open (dev, O_WRONLY);
	if (data->fd == -1)
		return FALSE;

	param = (32 << 16) | 0xC; /* 32 * 4096 */
	if (ioctl (data->fd, SNDCTL_DSP_SETFRAGMENT, &param) == -1)
		goto error;
	param = AFMT_S16_NE;
	if (ioctl (data->fd, SNDCTL_DSP_SETFMT, &param) == -1)
		goto error;
	param = 1;
	if (ioctl (data->fd, SNDCTL_DSP_STEREO, &param) == -1)
		goto error;
	param = 44100;
	if (ioctl (data->fd, SNDCTL_DSP_SPEED, &param) == -1)
		goto error;
	
	xmms_output_plugin_data_set (output, data);

	
	return TRUE;
error:
	close (data->fd);
	g_free (data);
	return FALSE;
}

static void
xmms_oss_close (xmms_output_t *output)
{
	xmms_oss_data_t *data;

	g_return_if_fail (output);
	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	close (data->fd);
}

static void
xmms_oss_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_oss_data_t *data;
	
	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	data = xmms_output_plugin_data_get (output);
	write (data->fd, buffer, len);
}
