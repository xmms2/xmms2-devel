
#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/magic.h"
#include "xmms/ringbuf.h"
#include "xmms/playlist_entry.h"

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

	gint running_handles, maxfd;
	guint length, bytes_since_meta, meta_offset; 
	
	fd_set fdread, fdwrite, fdexcp; 

	gchar *mime; 

	gboolean error, run, know_length, stream_with_meta, know_meta_offset; 

	struct curl_slist *http_aliases; 
	struct curl_slist *http_headers; 

	xmms_ringbuf_t *ringbuf; 

	GThread *thread; 
	GMutex *mutex; 

	xmms_playlist_entry_t *pl_entry; 
} xmms_curl_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_curl_can_handle (const gchar *url); 
static gboolean xmms_curl_init (xmms_transport_t *transport, const gchar *url); 
static void xmms_curl_close (xmms_transport_t *transport); 
static gint xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len); 
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
	return plugin; 
}

/*
 * Member functions
 */

static gboolean
xmms_curl_can_handle (const gchar *url)
{
	gchar *dec; 
	
	g_return_val_if_fail (url, FALSE); 

	dec = xmms_util_decode_path (url); 

	XMMS_DBG ("xmms_curl_can_handle (%s)", dec); 

	if ((g_strncasecmp (dec, "http", 4) == 0) || (dec[0] == '/')) {
		g_free (dec); 
		return TRUE; 
	}

	g_free (dec); 
	return FALSE; 
}

static gboolean
xmms_curl_init (xmms_transport_t *transport, const gchar *url)
{
	xmms_curl_data_t *data; 
	xmms_config_value_t *val; 
	gint bufsize, metaint; 

	XMMS_DBG ("xmms_curl_init ()"); 

	g_return_val_if_fail (transport, FALSE); 
	g_return_val_if_fail (url, FALSE); 

	data = g_new0 (xmms_curl_data_t, 1); 

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "buffersize"); 
	bufsize = xmms_config_value_int_get (val); 

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "shoutcastinfo"); 
	metaint = xmms_config_value_int_get (val); 
	
	data->ringbuf = xmms_ringbuf_new (bufsize); 
	data->mutex = g_mutex_new (); 
	data->pl_entry = xmms_playlist_entry_new (g_strdup (url)); 

	/* Set up easy handle */

	data->http_aliases = curl_slist_append (data->http_aliases, "ICY 200 OK"); 
	data->http_aliases = curl_slist_append (data->http_aliases, "ICY 402 Service Unavailabe"); 
	data->http_headers = curl_slist_append (data->http_headers, "Icy-MetaData: 1"); 

	data->curl_easy = curl_easy_init (); 

	curl_easy_setopt (data->curl_easy, CURLOPT_URL, xmms_util_decode_path (url)); 
	curl_easy_setopt (data->curl_easy, CURLOPT_HEADER, 0);	/* No, we _dont_ want headers in body */
	curl_easy_setopt (data->curl_easy, CURLOPT_HTTPGET, 1); 
	curl_easy_setopt (data->curl_easy, CURLOPT_FOLLOWLOCATION, 1);	/* Doesn't work in multi though... */
	curl_easy_setopt (data->curl_easy, CURLOPT_AUTOREFERER, 1); 
	curl_easy_setopt (data->curl_easy, CURLOPT_USERAGENT, "XMMS/" XMMS_VERSION); 
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEHEADER, transport); 
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEDATA, transport); 

	curl_easy_setopt (data->curl_easy, CURLOPT_HTTP200ALIASES, data->http_aliases); 
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEFUNCTION, xmms_curl_callback_write); 
	curl_easy_setopt (data->curl_easy, CURLOPT_HEADERFUNCTION, xmms_curl_callback_header); 

	if (metaint == 1) {
		curl_easy_setopt (data->curl_easy, CURLOPT_HTTPHEADER, data->http_headers);
		data->stream_with_meta = TRUE; 
	}

	/* For some debugging output set this to 1 */
	curl_easy_setopt (data->curl_easy, CURLOPT_VERBOSE, 0); 

	/* Set up multi handle */

	data->curl_multi = curl_multi_init (); 

	curl_multi_add_handle (data->curl_multi, data->curl_easy); 

	/* And add the final touch of complexity to this mess: start up another thread */

	xmms_transport_private_data_set (transport, data); 

	data->run = TRUE; 
	data->thread = g_thread_create ((GThreadFunc) xmms_curl_thread, (gpointer) transport, TRUE, NULL);
	g_return_val_if_fail (data->thread, FALSE); 

	/* Let's begin buffering too, just because it's so fun.. */

	xmms_transport_buffering_start (transport); 

	return TRUE; 
}

static gint
xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	xmms_curl_data_t *data;
	gchar *metadata;
	gint ret; 
	guchar funky; 

	//XMMS_DBG ("xmms_curl_read ()"); 

	g_return_val_if_fail (transport, -1); 
	g_return_val_if_fail (buffer, -1); 

	data = xmms_transport_private_data_get (transport); 
	g_return_val_if_fail (data, -1); 

	if (!data->run) {
		/* Thread ended... */
		return -1;
	}

	if (data->stream_with_meta && data->know_meta_offset) {
		if (data->bytes_since_meta == data->meta_offset) {
			gchar **tags;
			gint i = 0;

			data->bytes_since_meta = 0; 

			g_mutex_lock (data->mutex); 
			xmms_ringbuf_wait_used (data->ringbuf, 1, data->mutex);
			xmms_ringbuf_read (data->ringbuf, &funky, 1);
			g_mutex_unlock (data->mutex); 

			if (funky == 0)
				return 0; 

			metadata = g_malloc (funky * 16); 

			g_mutex_lock (data->mutex); 
			xmms_ringbuf_wait_used (data->ringbuf, funky * 16, data->mutex); 
			ret = xmms_ringbuf_read (data->ringbuf, metadata, funky * 16); 
			g_mutex_unlock (data->mutex); 

			XMMS_DBG ("METADATA: %s", metadata); 

			tags = g_strsplit (metadata, ";", 0);
			while (tags[i]) {
				if (g_strncasecmp (tags[i], "StreamTitle=", 12) == 0) {
					gint r,w;
					gchar *tmp, *tmp2;
					tmp = tags[i]+13;
					tmp[strlen (tmp)-1] = '\0';

					tmp2 = g_convert (tmp, strlen (tmp), "UTF-8", "ISO-8859-1", &r, &w, NULL);
					xmms_transport_mediainfo_property_set (transport,
									       XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE, 
									       tmp2);
					g_free (tmp2);
				}

				i++;
			}

			g_strfreev (tags);

			g_free (metadata); 
		}

		if (data->bytes_since_meta + len > data->meta_offset) {
			g_mutex_lock (data->mutex); 
			ret = xmms_ringbuf_read (data->ringbuf, buffer, data->meta_offset - data->bytes_since_meta); 
			g_mutex_unlock (data->mutex); 
			
			data->bytes_since_meta += ret; 

			return ret; 
		}

		g_mutex_lock (data->mutex); 
		ret = xmms_ringbuf_read (data->ringbuf, buffer, len);
		g_mutex_unlock (data->mutex); 

		data->bytes_since_meta += ret; 
	//	XMMS_DBG ("xmms_curl_read () returning %u bytes, bytes_since_meta = %u", 
	//					ret, data->bytes_since_meta); 
		return ret; 
	}

	g_mutex_lock (data->mutex); 
	ret = xmms_ringbuf_read (data->ringbuf, buffer, len); 
	g_mutex_unlock (data->mutex); 

	return ret; 
}

static gint
xmms_curl_size (xmms_transport_t *transport)
{
	xmms_curl_data_t *data; 

	g_return_val_if_fail (transport, -1); 

	data = xmms_transport_private_data_get (transport); 
	g_return_val_if_fail (data, -1); 

	if (!data->know_length)
		return -1; 

	return data->length; 
}

static void
xmms_curl_close (xmms_transport_t *transport)
{
	xmms_curl_data_t *data; 

	XMMS_DBG ("xmms_curl_close ()"); 

	g_return_if_fail (transport); 

	data = xmms_transport_private_data_get (transport); 
	g_return_if_fail (data); 

	data->run = FALSE; 

	g_mutex_lock (data->mutex); 
	xmms_ringbuf_clear (data->ringbuf); 
	g_mutex_unlock (data->mutex); 
	
	XMMS_DBG ("Ringbuffer cleared"); 
	
	XMMS_DBG ("Waiting for thread..."); 
	g_thread_join (data->thread); 
	XMMS_DBG ("Thread is joined"); 

	curl_multi_cleanup (data->curl_multi); 
	curl_easy_cleanup (data->curl_easy); 
	
	XMMS_DBG ("CURL cleaned up"); 
	
	xmms_ringbuf_clear (data->ringbuf); 
	xmms_ringbuf_destroy (data->ringbuf); 

	XMMS_DBG ("Ringbuffer destroyed"); 
	
	g_mutex_free (data->mutex); 

	curl_slist_free_all (data->http_aliases);
	curl_slist_free_all (data->http_headers);

	xmms_object_unref (data->pl_entry); 

	g_free (data->mime); 
	g_free (data); 

	XMMS_DBG ("All done!"); 
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
	xmms_ringbuf_wait_free (data->ringbuf, size * nmemb, data->mutex); 
	ret = xmms_ringbuf_write (data->ringbuf, ptr, size * nmemb); 
	g_mutex_unlock (data->mutex); 

	//XMMS_DBG ("xmms_curl_callback_write (%u), returning %u", size * nmemb, ret); 
	
	return ret; 
}

/* This can probably be made a lot better */

static size_t
xmms_curl_callback_header (void *ptr, size_t size, size_t nmemb, void *stream)
{
	xmms_transport_t *transport = (xmms_transport_t *) stream; 
	xmms_curl_data_t *data;
	gchar *header; 
	gchar *tmp;
	gint r,w;

	XMMS_DBG ("xmms_curl_callback_header ()"); 

	g_return_val_if_fail (transport, -1); 

	data = xmms_transport_private_data_get (transport); 
	g_return_val_if_fail (data, -1); 

	header = g_strndup (ptr, nmemb - 2); 

	if (g_strncasecmp (header, "content-type: ", 14) == 0) {
		data->mime = g_strdup (header + 14);
		xmms_transport_mimetype_set (transport, data->mime);
	}

	else if (g_strncasecmp (header, "content-type:", 13) == 0) {
		data->mime = g_strdup (header + 13);
		xmms_transport_mimetype_set (transport, data->mime);
	} 

	else if (g_strncasecmp (header, "content-length: ", 16) == 0) {
		data->know_length = TRUE; 
		data->length = atoi (header + 16);		/* stroul! */
	}

	else if (g_strncasecmp (header, "icy-metaint:", 12) == 0) {
		data->know_meta_offset = TRUE; 
		data->meta_offset = atoi (header + 12); 
		XMMS_DBG ("setting metaint to %d", data->meta_offset);
	} else if (g_strncasecmp (header, "icy-name:", 9) == 0) {
		tmp = g_convert (header+9, strlen (header+9), "UTF-8", "ISO-8859-1", &r, &w, NULL);
		xmms_transport_mediainfo_property_set (transport, XMMS_PLAYLIST_ENTRY_PROPERTY_CHANNEL, tmp);
		g_free (tmp);
	} else if (g_strncasecmp (header, "icy-br:", 7) == 0) {
		xmms_transport_mediainfo_property_set (transport, XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE, header+7);
	} else if (g_strncasecmp (header, "icy-genre:", 10) == 0) {
		tmp = g_convert (header+10, strlen (header+10), "UTF-8", "ISO-8859-1", &r, &w, NULL);
		xmms_transport_mediainfo_property_set (transport, XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE, tmp);
		g_free (tmp);
	}

	else {
		XMMS_DBG ("Unknown header: %s", header); 
	}

	xmms_transport_entry_mediainfo_set (transport, data->pl_entry); 

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

	XMMS_DBG ("xmms_curl_thread ()"); 

	g_return_if_fail (transport); 

	data = xmms_transport_private_data_get (transport); 
	g_return_if_fail (data); 

	FD_ZERO (&data->fdread); 
	FD_ZERO (&data->fdwrite); 
	FD_ZERO (&data->fdexcp); 

	while (curl_multi_perform (data->curl_multi, &data->running_handles) == CURLM_CALL_MULTI_PERFORM); 


	XMMS_DBG ("xmms_curl_thread is now running!"); 

	curl_multi_fdset (data->curl_multi, &data->fdread, &data->fdwrite, &data->fdexcp,  &data->maxfd);

	while (data->run && data->running_handles) {
		gint ret; 

		timeout.tv_sec = 1; 
		timeout.tv_usec = 0; 
		
		ret = select (data->maxfd + 1, &data->fdread, &data->fdwrite, &data->fdexcp, &timeout); 

		switch (ret) {
			case -1:
				XMMS_DBG ("Disconnected?"); 
				data->run = FALSE;
				continue; 
			case 0:
				continue; 
			default:
				curl_multi_perform (data->curl_multi, &data->running_handles);
				break; 
		}
	}
}
