#include "transport.h"
#include "plugin.h"
#include "object.h"
#include "util.h"
#include "ringbuf.h"

/*
 * Type definitions
 */

struct xmms_transport_St {
	xmms_object_t object;
	xmms_plugin_t *plugin;

	GMutex *mutex;
	GThread *thread;
	gboolean running;

	xmms_ringbuf_t *buffer;
	gchar *mime_type;
	gpointer plugin_data;
};

/*
 * Macros
 */

#define xmms_transport_lock(t) g_mutex_lock ((t)->mutex)
#define xmms_transport_unlock(t) g_mutex_unlock ((t)->mutex)


/*
 * Static function prototypes
 */

static xmms_plugin_t *xmms_transport_find_plugin (const gchar *uri);
static gpointer xmms_transport_thread (gpointer data);

/*
 * Public functions
 */

gpointer
xmms_transport_plugin_data_get (xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, NULL);

	return transport->plugin_data;
}

void
xmms_transport_plugin_data_set (xmms_transport_t *transport, gpointer data)
{
	transport->plugin_data = data;
}

void
xmms_transport_mime_type_set (xmms_transport_t *transport, const gchar *mimetype)
{
	g_return_if_fail (transport);
	g_return_if_fail (mimetype);

	if (transport->mime_type)
		g_free (transport->mime_type);
	transport->mime_type = g_strdup (mimetype);

	xmms_object_emit (XMMS_OBJECT (transport), "mime-type-changed", mimetype);
}


/*
 * Private functions
 */

xmms_transport_t *
xmms_transport_open (const gchar *uri)
{
	xmms_plugin_t *plugin;
	xmms_transport_t *transport;
	xmms_transport_open_method_t open_method;

	g_return_val_if_fail (uri, NULL);

	XMMS_DBG ("Trying to open stream: %s", uri);
	
	plugin = xmms_transport_find_plugin (uri);
	if (!plugin)
		return NULL;
	
	XMMS_DBG ("Found plugin: %s", xmms_plugin_name_get (plugin));

	transport = g_new0 (xmms_transport_t, 1);
	xmms_object_init (XMMS_OBJECT (transport));
	transport->plugin = plugin;
	transport->mutex = g_mutex_new ();
	transport->buffer = xmms_ringbuf_new (32768);

	open_method = xmms_plugin_method_get (plugin, "open");

	if (!open_method || !open_method (transport, uri)) {
		XMMS_DBG ("Open failed");
		xmms_ringbuf_destroy (transport->buffer);
		g_mutex_free (transport->mutex);
		g_free (transport);
	}
	
	return transport;
}

const gchar *
xmms_transport_mime_type_get (xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, NULL);

	return transport->mime_type;
}

void
xmms_transport_start (xmms_transport_t *transport)
{
	g_return_if_fail (transport);

	transport->running = TRUE;
	transport->thread = g_thread_create (xmms_transport_thread, transport, TRUE, NULL); 
}

void
xmms_transport_wait (xmms_transport_t *transport)
{

	if (transport->running)
		g_thread_join (transport->thread);

	transport->running = FALSE;

	return;

}

gint
xmms_transport_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (len > 0, -1);
	
	xmms_transport_lock (transport);

	xmms_ringbuf_wait_used (transport->buffer, len, transport->mutex);
	ret = xmms_ringbuf_read (transport->buffer, buffer, len);
	
	xmms_transport_unlock (transport);

	return ret;
}

/*
 * Static functions
 */

static xmms_plugin_t *
xmms_transport_find_plugin (const gchar *uri)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;
	xmms_transport_can_handle_method_t can_handle;

	g_return_val_if_fail (uri, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_TRANSPORT);
	XMMS_DBG ("List: %p", list);
	
	for (node = list; node; node = g_list_next (node)) {
		plugin = node->data;
		XMMS_DBG ("Trying plugin: %s", xmms_plugin_name_get (plugin));
		can_handle = xmms_plugin_method_get (plugin, "can_handle");
		
		if (!can_handle)
			continue;

		if (can_handle (uri)) {
			xmms_plugin_ref (plugin);
			break;
		}
	}
	if (!node)
		plugin = NULL;

	if (list)
		xmms_plugin_list_destroy (list);

	return plugin;
}

static gpointer
xmms_transport_thread (gpointer data)
{
	xmms_transport_t *transport = data;
	gchar buffer[4096];
	xmms_transport_read_method_t read_method;
	xmms_transport_eof_method_t eof_method;
	gint ret;

	g_return_val_if_fail (transport, NULL);
	
	read_method = xmms_plugin_method_get (transport->plugin, "read");
	if (!read_method)
		return NULL;

	eof_method = xmms_plugin_method_get (transport->plugin, "eof");
	
	while (transport->running) {
		xmms_transport_lock (transport);

		ret = read_method (transport, buffer, 4096);
		XMMS_DBG ("read %d bytes", ret);

		if (ret > 0) {
			xmms_ringbuf_wait_free (transport->buffer, ret, transport->mutex);
			xmms_ringbuf_write (transport->buffer, buffer, ret);
		} else if (eof_method (transport)) {
			transport->running = FALSE;
		}
		xmms_transport_unlock (transport);
	}

	return NULL;
}
