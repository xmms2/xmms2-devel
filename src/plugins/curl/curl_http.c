
#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/magic.h"
#include "xmms/ringbuf.h"
#include "xmms/medialib.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <string.h>

#include <curl/curl.h>

/*
 * Type definitions
 */

typedef struct {
	CURL *curl_easy;
	CURLM *curl_multi;

	guint length, bytes_since_meta, meta_offset;

	gchar *url;

	gboolean have_headers, running, know_length, know_mime, shoutcast;

	struct curl_slist *http_aliases;
	struct curl_slist *http_headers;

	xmms_ringbuf_t *ringbuf;

	GThread *thread;
	GMutex *mutex;

	xmms_error_t status;
} xmms_curl_data_t;

typedef void (*handler_func_t) (xmms_transport_t *transport, gchar *header);

static void header_handler_contenttype (xmms_transport_t *transport, gchar *header);
static void header_handler_contentlength (xmms_transport_t *transport, gchar *header);
static void header_handler_icy_metaint (xmms_transport_t *transport, gchar *header);
static void header_handler_icy_name (xmms_transport_t *transport, gchar *header);
static void header_handler_icy_genre (xmms_transport_t *transport, gchar *header);
static void header_handler_icy_br (xmms_transport_t *transport, gchar *header);
static void header_handler_icy_ok (xmms_transport_t *transport, gchar *header);
static void header_handler_last (xmms_transport_t *transport, gchar *header);
static handler_func_t header_handler_find (gchar *header);
static void handle_shoutcast_metadata (xmms_transport_t *transport, gchar *metadata);

typedef struct {
	gchar *name;
	handler_func_t func;
} handler_t;

handler_t handlers[] = {
	{ "content-type", header_handler_contenttype },
	{ "content-length", header_handler_contentlength },
	{ "icy-metaint", header_handler_icy_metaint },
	{ "icy-name", header_handler_icy_name },
	{ "icy-genre", header_handler_icy_genre },
	{ "ICY 200 OK", header_handler_icy_ok },
	{ "icy-br", header_handler_icy_br },
	{ "\r\n", header_handler_last },
	{ NULL, NULL }
};

/*
 * Function prototypes
 */

static gboolean xmms_curl_can_handle (const gchar *url);
static gboolean xmms_curl_init (xmms_transport_t *transport, const gchar *url);
static void xmms_curl_close (xmms_transport_t *transport);
static gint xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error);
static gint xmms_curl_size (xmms_transport_t *transport);
static size_t xmms_curl_callback_write (void *ptr, size_t size, size_t nmemb, void *stream);
static size_t xmms_curl_callback_header (void *ptr, size_t size, size_t nmemb, void *stream);
static void xmms_curl_thread (xmms_transport_t *transport);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "curl_http",
	                          "Curl transport for HTTP " XMMS_VERSION,
	                          "HTTP transport using CURL");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org");
	xmms_plugin_info_add (plugin, "INFO", "http://curl.haxx.se/libcurl");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_curl_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_curl_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_curl_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_curl_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_curl_close);

	xmms_plugin_config_value_register (plugin, "shoutcastinfo", "1", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "buffersize", "131072", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "verbose", "0", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "connecttimeout", "15", NULL, NULL);

	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_curl_can_handle (const gchar *url)
{
	g_return_val_if_fail (url, FALSE);

	if ((g_strncasecmp (url, "http", 4) == 0) || (url[0] == '/')) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
xmms_curl_init (xmms_transport_t *transport, const gchar *url)
{
	xmms_curl_data_t *data;
	xmms_config_value_t *val;
	gint bufsize, metaint, verbose, connecttimeout;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	data = g_new0 (xmms_curl_data_t, 1);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "buffersize");
	bufsize = xmms_config_value_int_get (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "connecttimeout");
	connecttimeout = xmms_config_value_int_get (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "shoutcastinfo");
	metaint = xmms_config_value_int_get (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "verbose");
	verbose = xmms_config_value_int_get (val);

	data->ringbuf = xmms_ringbuf_new (bufsize);
	data->mutex = g_mutex_new ();
	data->url = g_strdup (url);

	data->http_aliases = curl_slist_append (data->http_aliases, "ICY 200 OK");
	data->http_aliases = curl_slist_append (data->http_aliases, "ICY 402 Service Unavailabe");
	data->http_headers = curl_slist_append (data->http_headers, "Icy-MetaData: 1");

	data->curl_easy = curl_easy_init ();

	curl_easy_setopt (data->curl_easy, CURLOPT_URL, data->url);
	curl_easy_setopt (data->curl_easy, CURLOPT_HEADER, 0);
	curl_easy_setopt (data->curl_easy, CURLOPT_HTTPGET, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_USERAGENT, "XMMS/" XMMS_VERSION);
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEHEADER, transport);
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEDATA, transport);
	curl_easy_setopt (data->curl_easy, CURLOPT_HTTP200ALIASES, data->http_aliases);
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEFUNCTION, xmms_curl_callback_write);
	curl_easy_setopt (data->curl_easy, CURLOPT_HEADERFUNCTION, xmms_curl_callback_header);
	curl_easy_setopt (data->curl_easy, CURLOPT_CONNECTTIMEOUT, connecttimeout);
	curl_easy_setopt (data->curl_easy, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_VERBOSE, verbose);

	if (metaint == 1)
		curl_easy_setopt (data->curl_easy, CURLOPT_HTTPHEADER, data->http_headers);

	data->curl_multi = curl_multi_init ();

	curl_multi_add_handle (data->curl_multi, data->curl_easy);

	xmms_transport_private_data_set (transport, data);

	data->running = TRUE;
	data->thread = g_thread_create ((GThreadFunc) xmms_curl_thread, (gpointer) transport, TRUE, NULL);
	g_return_val_if_fail (data->thread, FALSE);

	return TRUE;
}

static gint
xmms_curl_read_with_meta (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error)
{
	xmms_curl_data_t *data;
	guchar magic = 0;
	gchar *metadata;
	gint ret;

	data = xmms_transport_private_data_get (transport);

	if (data->bytes_since_meta == data->meta_offset) {
		data->bytes_since_meta = 0;

		xmms_ringbuf_read_wait (data->ringbuf, &magic, 1, data->mutex);

		if (magic != 0) {

			metadata = g_malloc0 (magic * 16);
			xmms_ringbuf_read_wait (data->ringbuf, metadata, magic * 16, data->mutex);

			handle_shoutcast_metadata (transport, metadata);
			g_free (metadata);

		}
	}

	if (data->bytes_since_meta + len > data->meta_offset)
		len = data->meta_offset - data->bytes_since_meta;

	ret = xmms_ringbuf_read_wait (data->ringbuf, buffer, len, data->mutex);
	data->bytes_since_meta += ret;

	return ret;
}

static gint
xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error)
{
	xmms_curl_data_t *data;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	g_mutex_lock (data->mutex);

	xmms_ringbuf_wait_used (data->ringbuf, 1, data->mutex);

	if (xmms_ringbuf_iseos (data->ringbuf)) {

		if (data->status.code == XMMS_ERROR_EOS)
			ret = 0;
		else
			ret = -1;

		xmms_error_set (error, data->status.code, data->status.message);

		g_mutex_unlock (data->mutex);
		return ret;
	}

	if (data->meta_offset > 0)
		ret = xmms_curl_read_with_meta (transport, buffer, len, error);
	else
		ret = xmms_ringbuf_read_wait (data->ringbuf, buffer, len, data->mutex);

	g_mutex_unlock (data->mutex);
	return ret;
}

static gint
xmms_curl_size (xmms_transport_t *transport)
{
	xmms_curl_data_t *data;
	gint len;

	g_return_val_if_fail (transport, -1);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	g_mutex_lock (data->mutex);
	if (data->know_length)
		len = data->length;
	else
		len = -1;
	g_mutex_unlock (data->mutex);

	return len;
}

static void
xmms_curl_close (xmms_transport_t *transport)
{
	xmms_curl_data_t *data;

	g_return_if_fail (transport);

	data = xmms_transport_private_data_get (transport);
	g_return_if_fail (data);

	g_mutex_lock (data->mutex);
	data->running = FALSE;
	xmms_ringbuf_clear (data->ringbuf);
	g_mutex_unlock (data->mutex);

	XMMS_DBG ("Waiting for thread...");
	g_thread_join (data->thread);
	XMMS_DBG ("Thread is joined");

	curl_multi_cleanup (data->curl_multi);
	curl_easy_cleanup (data->curl_easy);

	xmms_ringbuf_clear (data->ringbuf);
	xmms_ringbuf_destroy (data->ringbuf);

	g_mutex_free (data->mutex);

	curl_slist_free_all (data->http_aliases);
	curl_slist_free_all (data->http_headers);

	g_free (data->url);
	g_free (data);
}

/*
 * CURL callback functions
 */

static size_t
xmms_curl_callback_write (void *ptr, size_t size, size_t nmemb, void *stream)
{
	xmms_curl_data_t *data;
	xmms_transport_t *transport = (xmms_transport_t *) stream;
	gint ret;

	g_return_val_if_fail (transport, 0);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, 0);

	g_mutex_lock (data->mutex);
	ret = xmms_ringbuf_write_wait (data->ringbuf, ptr, size * nmemb, data->mutex);
	g_mutex_unlock (data->mutex);

	return ret;
}

static size_t
xmms_curl_callback_header (void *ptr, size_t size, size_t nmemb, void *stream)
{
	xmms_transport_t *transport = (xmms_transport_t *) stream;
	handler_func_t func;
	gchar *header;

	g_return_val_if_fail (transport, -1);

	header = g_strndup ((gchar*)ptr, size * nmemb);

	func = header_handler_find (header);
	if (func != NULL) {
		gchar *val = header + strcspn (header, ":") + 1;

		g_strstrip (val);
		func (transport, val);
	}

	g_free (header);
	return size * nmemb;
}

/*
 * Our curl thread
 */

static void
xmms_curl_thread (xmms_transport_t *transport)
{
	xmms_curl_data_t *data;
	struct timeval timeout;
	gint handles, maxfd;

	fd_set fdread, fdwrite, fdexcp, *fdtemp;

	g_return_if_fail (transport);

	data = xmms_transport_private_data_get (transport);
	g_return_if_fail (data);

	FD_ZERO (&fdread);
	FD_ZERO (&fdwrite);
	FD_ZERO (&fdexcp);

	while (curl_multi_perform (data->curl_multi, &handles) == CURLM_CALL_MULTI_PERFORM);

	g_mutex_lock (data->mutex);
	while (data->running && handles) {
		CURLMcode code;
		gint ret;

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		if (data->have_headers)
			fdtemp = NULL;
		else
			fdtemp = &fdwrite;

		curl_multi_fdset (data->curl_multi, &fdread, &fdwrite, &fdexcp,  &maxfd);

		g_mutex_unlock (data->mutex);
		ret = select (maxfd + 1, &fdread, fdtemp, &fdexcp, &timeout);
		g_mutex_lock (data->mutex);

		if (ret == -1)
			goto cont;

		if (ret == 0)
			continue;

		while (42) {
			g_mutex_unlock (data->mutex);
			code = curl_multi_perform (data->curl_multi, &handles);
			g_mutex_lock (data->mutex);

			if (code == CURLM_OK)
				break;

			if (code != CURLM_CALL_MULTI_PERFORM) {
				xmms_log_error ("%s", curl_multi_strerror (code));
				goto cont;
			}
		}
	}

cont:
	if (!data->know_mime) {
		xmms_transport_mimetype_set (transport, NULL);
	}

	xmms_ringbuf_set_eos (data->ringbuf, TRUE);
	g_mutex_unlock (data->mutex);
}

static handler_func_t
header_handler_find (gchar *header)
{
	guint i;

	g_return_val_if_fail (header, NULL);

	for (i = 0; handlers[i].name != NULL; i++) {
		guint len = strlen (handlers[i].name);

		if (g_ascii_strncasecmp (handlers[i].name, header, len) == 0)
			return handlers[i].func;
	}

	return NULL;
}

static void
header_handler_contenttype (xmms_transport_t *transport, gchar *header)
{
	xmms_curl_data_t *data;

	data = xmms_transport_private_data_get (transport);

	g_mutex_lock (data->mutex);
	data->know_mime = TRUE;
	g_mutex_unlock (data->mutex);

	xmms_transport_mimetype_set (transport, header);
}

static void
header_handler_contentlength (xmms_transport_t *transport, gchar *header)
{
	xmms_curl_data_t *data;

	data = xmms_transport_private_data_get (transport);

	g_mutex_lock (data->mutex);
	data->length = strtoul (header, NULL, 10);
	g_mutex_unlock (data->mutex);
}

static void
header_handler_icy_metaint (xmms_transport_t *transport, gchar *header)
{
	xmms_curl_data_t *data;

	data = xmms_transport_private_data_get (transport);

	g_mutex_lock (data->mutex);
	data->meta_offset = strtoul (header, NULL, 10);
	g_mutex_unlock (data->mutex);
}

static void
header_handler_icy_name (xmms_transport_t *transport, gchar *header)
{
	xmms_medialib_entry_t entry;

	entry = xmms_transport_medialib_entry_get (transport);
	xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNEL, header);
	xmms_medialib_entry_send_update (entry);
}

static void
header_handler_icy_br (xmms_transport_t *transport, gchar *header)
{
	xmms_medialib_entry_t entry;

	entry = xmms_transport_medialib_entry_get (transport);
	xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE, header);
	xmms_medialib_entry_send_update (entry);
}

static void
header_handler_icy_ok (xmms_transport_t *transport, gchar *header)
{
	xmms_curl_data_t *data;

	data = xmms_transport_private_data_get (transport);

	g_mutex_lock (data->mutex);
	data->shoutcast = TRUE;
	g_mutex_unlock (data->mutex);
}

static void
header_handler_icy_genre (xmms_transport_t *transport, gchar *header)
{
	xmms_medialib_entry_t entry;

	entry = xmms_transport_medialib_entry_get (transport);
	xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE, header);
	xmms_medialib_entry_send_update (entry);
}

static void
header_handler_last (xmms_transport_t *transport, gchar *header)
{
	xmms_curl_data_t *data;
	gchar *mime = NULL;

	data = xmms_transport_private_data_get (transport);

	g_mutex_lock (data->mutex);

	if (data->shoutcast)
		mime = "audio/mpeg";

	if (!data->know_mime)
		xmms_transport_mimetype_set (transport, mime);

	data->know_mime = TRUE;
	data->have_headers = TRUE;
	g_mutex_unlock (data->mutex);
}

static void
handle_shoutcast_metadata (xmms_transport_t *transport, gchar *metadata)
{
	xmms_medialib_entry_t entry;
	xmms_curl_data_t *data;
	gchar **tags;
	guint i = 0;

	data = xmms_transport_private_data_get (transport);
	entry = xmms_transport_medialib_entry_get (transport);

	if (!metadata) {
		return;
	}

	tags = g_strsplit (metadata, ";", 0);
	while (tags[i] != NULL) {
		if (g_strncasecmp (tags[i], "StreamTitle=", 12) == 0) {
			gchar *raw;

			raw = tags[i] + 13;
			raw[strlen (raw) - 1] = '\0';

			xmms_medialib_entry_property_set (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, raw);
			xmms_medialib_entry_send_update (entry);
		}

		i++;
	}
	g_strfreev (tags);
}

