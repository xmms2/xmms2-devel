#include "transport.h"
#include "plugin.h"
#include "object.h"
#include "util.h"
#include "ringbuf.h"

/*
 * Static function prototypes
 */

static void xmms_transport_destroy (xmms_transport_t *transport);
static xmms_plugin_t *xmms_transport_find_plugin (const gchar *uri);
static gpointer xmms_transport_thread (gpointer data);

/*
 * Public functions
 */

gpointer
xmms_transport_plugin_data_get (xmms_transport_t *transport)
{
	gpointer ret;
	g_return_val_if_fail (transport, NULL);

	ret = transport->plugin_data;

	return ret;
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

	xmms_transport_lock (transport);
	
	if (transport->mime_type)
		g_free (transport->mime_type);
	transport->mime_type = g_strdup (mimetype);

	xmms_transport_unlock (transport);
	
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
	transport->cond = g_cond_new ();
	transport->seek_cond = g_cond_new ();
	transport->buffer = xmms_ringbuf_new (XMMS_TRANSPORT_RINGBUF_SIZE);
	transport->want_seek = FALSE;

	open_method = xmms_plugin_method_get (plugin, XMMS_METHOD_OPEN);

	if (!open_method || !open_method (transport, uri)) {
		XMMS_DBG ("Open failed");
		xmms_ringbuf_destroy (transport->buffer);
		g_mutex_free (transport->mutex);
		g_free (transport);
		transport = NULL;
	}
	
	return transport;
}

void
xmms_transport_close (xmms_transport_t *transport)
{
	g_return_if_fail (transport);

	if (transport->thread) {
		xmms_transport_lock (transport);
		transport->running = FALSE;
		g_cond_signal (transport->cond);
		xmms_transport_unlock (transport);
	} else {
		xmms_transport_destroy (transport);
	}
}

const gchar *
xmms_transport_mime_type_get (xmms_transport_t *transport)
{
	const gchar *ret;
	g_return_val_if_fail (transport, NULL);

	xmms_transport_lock (transport);
	ret =  transport->mime_type;
	xmms_transport_unlock (transport);

	return ret;
}

void
xmms_transport_start (xmms_transport_t *transport)
{
	g_return_if_fail (transport);

	transport->running = TRUE;
	transport->thread = g_thread_create (xmms_transport_thread, transport, TRUE, NULL); 
}

gint
xmms_transport_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (len > 0, -1);

	xmms_transport_lock (transport);
	
	if (transport->want_seek) {
		g_cond_wait (transport->seek_cond, transport->mutex);
	}

	if (len > XMMS_TRANSPORT_RINGBUF_SIZE) {
		len = XMMS_TRANSPORT_RINGBUF_SIZE;
	}

	xmms_ringbuf_wait_used (transport->buffer, len, transport->mutex);
	ret = xmms_ringbuf_read (transport->buffer, buffer, len);
	
	xmms_transport_unlock (transport);

	return ret;
}

void
xmms_transport_seek (xmms_transport_t *transport, gint offset, gint whence)
{
	g_return_if_fail (transport);
	g_return_if_fail (!transport->want_seek);

	transport->seek_offset = offset;
	transport->seek_whence = whence;
	transport->want_seek = TRUE;

	xmms_ringbuf_set_eos (transport->buffer, TRUE);
	g_cond_signal (transport->cond);
}

gint
xmms_transport_size (xmms_transport_t *transport)
{
	xmms_transport_size_method_t size;
	g_return_val_if_fail (transport, -1);

	size = xmms_plugin_method_get (transport->plugin, XMMS_METHOD_SIZE);
	g_return_val_if_fail (size, -1);

	return size (transport);
}

/*
 * Static functions
 */

static void
xmms_transport_seek_real (xmms_transport_t *transport)
{
	xmms_transport_seek_method_t seek_method;
	g_return_if_fail (transport);

	seek_method = xmms_plugin_method_get (transport->plugin, XMMS_METHOD_SEEK);
	g_return_if_fail (seek_method);

	xmms_ringbuf_clear (transport->buffer);
	seek_method (transport, transport->seek_offset, transport->seek_whence);

	transport->want_seek = FALSE;
	xmms_ringbuf_set_eos (transport->buffer, FALSE);
	g_cond_broadcast (transport->seek_cond);
}

static void
xmms_transport_destroy (xmms_transport_t *transport)
{
	xmms_transport_close_method_t close_method;

	close_method = xmms_plugin_method_get (transport->plugin, XMMS_METHOD_CLOSE);
	
	if (close_method)
		close_method (transport);
	xmms_ringbuf_destroy (transport->buffer);
	g_cond_free (transport->seek_cond);
	g_cond_free (transport->cond);
	g_mutex_free (transport->mutex);
	xmms_object_cleanup (XMMS_OBJECT (transport));
	g_free (transport);
}

static xmms_plugin_t *
xmms_transport_find_plugin (const gchar *uri)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;
	xmms_transport_can_handle_method_t can_handle;

	g_return_val_if_fail (uri, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_TRANSPORT);
	
	for (node = list; node; node = g_list_next (node)) {
		plugin = node->data;
		XMMS_DBG ("Trying plugin: %s", xmms_plugin_name_get (plugin));
		can_handle = xmms_plugin_method_get (plugin, XMMS_METHOD_CAN_HANDLE);
		
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
	gint ret;

	g_return_val_if_fail (transport, NULL);
	
	read_method = xmms_plugin_method_get (transport->plugin, XMMS_METHOD_READ);
	if (!read_method)
		return NULL;

	xmms_transport_lock (transport);
	while (transport->running) {
		if (transport->want_seek) {
			xmms_transport_seek_real (transport);
		}

		xmms_transport_unlock (transport);
		ret = read_method (transport, buffer, 4096);
		xmms_transport_lock (transport);
		if (ret > 0) {
			xmms_ringbuf_wait_free (transport->buffer, ret, transport->mutex);
			xmms_ringbuf_write (transport->buffer, buffer, ret);
		} else {
			xmms_ringbuf_set_eos (transport->buffer, TRUE);
			g_cond_wait (transport->cond, transport->mutex);
		}
	}
	xmms_transport_unlock (transport);


	XMMS_DBG ("xmms_transport_thread: cleaning up");
	
	xmms_transport_destroy (transport);
	
	return NULL;
}
