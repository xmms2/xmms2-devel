#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

/*
 * Type definitions
 */

typedef struct {
	gint fd;
} xmms_file_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_file_can_handle (const gchar *uri);
static gboolean xmms_file_open (xmms_transport_t *transport, const gchar *uri);
static gint xmms_file_read (xmms_transport_t *transport, gchar *buffer, guint len);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "File transport " VERSION,
							  "Plain file transport");
	
	xmms_plugin_method_add (plugin, "can_handle", xmms_file_can_handle);
	xmms_plugin_method_add (plugin, "open", xmms_file_open);
	xmms_plugin_method_add (plugin, "read", xmms_file_read);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_file_can_handle (const gchar *uri)
{
	g_return_val_if_fail (uri, FALSE);

	XMMS_DBG ("xmms_file_can_handle (%s)", uri);
	
	if ((g_strncasecmp (uri, "file:", 5) == 0) || (uri[0] == '/'))
		return TRUE;
	return FALSE;
}

static gboolean
xmms_file_open (xmms_transport_t *transport, const gchar *uri)
{
	gint fd;
	xmms_file_data_t *data;
	const gchar *uriptr;

	XMMS_DBG ("xmms_file_open (%p, %s)", transport, uri);
	
	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (uri, FALSE);

	if (g_strncasecmp (uri, "file:", 5) == 0)
		uriptr = strchr (uri, '/');
	else
		uriptr = uri;
	if (!uriptr)
		return FALSE;

	XMMS_DBG ("Opening %s", uriptr);
	fd = open (uriptr, O_RDONLY | O_NONBLOCK);
	XMMS_DBG ("fd: %d", fd);
	if (fd == -1)
		return FALSE;

	data = g_new0 (xmms_file_data_t, 1);
	data->fd = fd;
	xmms_transport_plugin_data_set (transport, data);

	xmms_transport_mime_type_set (transport, "audio/mpeg"); /* FIXME */
	
	return TRUE;
}

static gint
xmms_file_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	xmms_file_data_t *data;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	data = xmms_transport_plugin_data_get (transport);
	g_return_val_if_fail (data, -1);

	ret = read (data->fd, buffer, len);

	return ret;
}
