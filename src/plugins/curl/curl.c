/** @file interface to libcurl.
  *
  * This plugin will provide HTTP/HTTPS/FTP transports.
  */

#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/magic.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#include <curl/curl.h>

/*
 * Type definitions
 */

typedef struct {
	CURL *curl;
	CURLM *curlm;

	gint running;
	gboolean again;

	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;

	gint maxfd;

	gchar *buf;
	gint data_in_buf;

	gchar *mime;
	gchar *name;
	gchar *genre;
} xmms_curl_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_curl_can_handle (const gchar *uri);
static gboolean xmms_curl_init (xmms_transport_t *transport, const gchar *uri);
static void xmms_curl_close (xmms_transport_t *transport);
static gint xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len);
static gint xmms_curl_size (xmms_transport_t *transport);
static gint xmms_curl_seek (xmms_transport_t *transport, guint offset, gint whence);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "curl",
			"Curl transport for HTTP " XMMS_VERSION,
		 	"HTTP transport using CURL");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "INFO", "http://curl.haxx.se/libcurl/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_curl_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_curl_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_curl_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_curl_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_curl_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_curl_seek);

	//xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SEEK);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_curl_can_handle (const gchar *uri)
{
	gchar *dec;
	g_return_val_if_fail (uri, FALSE);

	dec = xmms_util_decode_path (uri);

	XMMS_DBG ("xmms_curl_can_handle (%s)", dec);
	
	if ((g_strncasecmp (dec, "http:", 5) == 0) || (dec[0] == '/')) {
		g_free (dec);
		return TRUE;
	}

	g_free (dec);
	return FALSE;
}

static size_t 
xmms_curl_cwrite (void *ptr, size_t size, size_t nmemb, void  *stream)
{
	xmms_curl_data_t *data = (xmms_curl_data_t *) stream;

	if (!data->mime) {
		curl_easy_getinfo (data->curl, CURLINFO_CONTENT_TYPE, &data->mime);
		XMMS_DBG ("mimetype here is %s", data->mime);
		
		if (!data->mime && g_strncasecmp (ptr, "ICY 200 OK", 10) == 0) {
			gchar **tmp;
			gint i=0;

			XMMS_DBG ("Shoutcast detected...");
			data->mime = "audio/mpeg"; /* quite safe */

/*			tmp = g_strsplit (ptr, "\r\n", 0);
			while (tmp[i]) {
				XMMS_DBG ("%s", tmp[i]);
				if (g_strncasecmp (tmp[i], "icy-name:", 9) == 0)
					data->name = g_strdup (tmp[i]+9);
				if (g_strncasecmp (tmp[i], "icy-genre:", 10) == 0)
					data->genre = g_strdup (tmp[i]+10);
				i++;
			}

			XMMS_DBG ("Got %s that plays %s", data->name, data->genre);

			g_strfreev (tmp);*/

			return size*nmemb;
		}
	}
	
	data->buf = g_malloc (size*nmemb);
	data->data_in_buf = size*nmemb;
	memcpy (data->buf, ptr, size*nmemb);

	return size*nmemb;
}

static gboolean
xmms_curl_init (xmms_transport_t *transport, const gchar *uri)
{
	xmms_curl_data_t *data;
	struct curl_slist *headerlist = NULL;
	
	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (uri, FALSE);

	data = g_new0 (xmms_curl_data_t, 1);

	data->curl = curl_easy_init ();
	data->curlm = curl_multi_init ();
	data->mime = NULL;
	
	g_return_val_if_fail (data->curl, FALSE);
	g_return_val_if_fail (data->curlm, FALSE);

	XMMS_DBG ("Setting up CURL");

	headerlist = curl_slist_append (headerlist, "Icy-MetaData: 1");

	curl_easy_setopt (data->curl, CURLOPT_URL, xmms_util_decode_path (uri));
	curl_easy_setopt (data->curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt (data->curl, CURLOPT_WRITEFUNCTION, xmms_curl_cwrite);
	curl_easy_setopt (data->curl, CURLOPT_WRITEDATA, data);
	curl_easy_setopt (data->curl, CURLOPT_HTTPGET, 1);
	curl_easy_setopt (data->curl, CURLOPT_USERAGENT, "XMMS/" XMMS_VERSION);
	curl_easy_setopt (data->curl, CURLOPT_HTTPHEADER, headerlist);

	curl_multi_add_handle (data->curlm, data->curl);

	if (curl_multi_perform (data->curlm, &data->running) == 
			CURLM_CALL_MULTI_PERFORM) {
		data->again = TRUE;
	}
	
	xmms_transport_plugin_data_set (transport, data);
	
	/** @todo mimetype from header? */
	xmms_transport_mime_type_set (transport, "audio/mpeg");

	FD_ZERO (&data->fdread);
	FD_ZERO (&data->fdwrite);
	FD_ZERO (&data->fdexcep);
	
	return TRUE;
}

static void
xmms_curl_close (xmms_transport_t *transport)
{
	xmms_curl_data_t *data;

	g_return_if_fail (transport);
	
	data = xmms_transport_plugin_data_get (transport);
	g_return_if_fail (data);
	
	curl_multi_cleanup (data->curlm);
	curl_easy_cleanup (data->curl);
}

static gint
xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	xmms_curl_data_t *data;
	struct timeval timeout;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);

	data = xmms_transport_plugin_data_get (transport);

	g_return_val_if_fail (data, -1);

	if (data->data_in_buf) {
		gint ret;

		if (len < data->data_in_buf) {
			gchar *tmp;

			tmp = data->buf;
			memcpy (buffer, data->buf, len);

			data->buf = g_malloc (data->data_in_buf-len);
			memcpy (data->buf, tmp+len, data->data_in_buf-len);
			data->data_in_buf = data->data_in_buf - len;

			g_free (tmp);

			return len;
		}

		memcpy (buffer, data->buf, data->data_in_buf);
		g_free (data->buf);
		ret = data->data_in_buf;
		data->data_in_buf = 0;

		return ret;
	}

	if (!data->running)
		return -1;
		
	if (data->again) {
		if (curl_multi_perform (data->curlm, &data->running) ==
				CURLM_CALL_MULTI_PERFORM) {
			data->again = TRUE;
			return 0;
		}
		data->again = FALSE;
		if (!data->maxfd)
			curl_multi_fdset (data->curlm, &data->fdread, &data->fdwrite, &data->fdexcep, &data->maxfd);
		return 0;
	}
	
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	ret = select (data->maxfd+1, &data->fdread, &data->fdwrite, &data->fdexcep, &timeout);

	switch (ret) {
		case -1:
			return -1;
		case 0:
			return 0;
		default:
			if (curl_multi_perform (data->curlm, &data->running) ==
					CURLM_CALL_MULTI_PERFORM) {
				data->again = TRUE;
			}
			return 0;
	}

	return -1;
}

static gint
xmms_curl_seek (xmms_transport_t *transport, guint offset, gint whence)
{
	return 0;
}

static gint
xmms_curl_size (xmms_transport_t *transport)
{
	return 0;
}

