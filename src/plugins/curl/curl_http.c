/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *                   
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */




/** @file interface to libcurl.
  *
  * This plugin will provide HTTP/HTTPS/FTP transports.
  */

#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/magic.h"
#include "xmms/ringbuf.h"

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
	CURL *curl;
	CURLM *curlm;

	gint running;
	gboolean run;

	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;
	gint maxfd;

	xmms_ringbuf_t *buffer;

	gboolean stream;
	gchar *mime;
	gchar *name;
	gchar *genre;
	gchar *br;
	gint metaint;

	gchar *url;

	GThread *thread;
	GMutex *mutex;
} xmms_curl_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_curl_can_handle (const gchar *url);
static gboolean xmms_curl_init (xmms_transport_t *transport, const gchar *url);
static void xmms_curl_close (xmms_transport_t *transport);
static gint xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len);
static gint xmms_curl_size (xmms_transport_t *transport);
/*static gint xmms_curl_seek (xmms_transport_t *transport, guint offset, gint whence);*/

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

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "INFO", "http://curl.haxx.se/libcurl/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_curl_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_curl_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_curl_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_curl_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_curl_size);
	//xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_curl_seek);

	//xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SEEK);

	xmms_plugin_config_value_register (plugin, "buffersize", "131072", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "prebuffersize", "32768", NULL, NULL);
	xmms_plugin_config_value_register (plugin, "shoutcastinfo", "0", NULL, NULL);
	
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
	
	if ((g_strncasecmp (dec, "http:", 5) == 0) || (dec[0] == '/')) {
		g_free (dec);
		return TRUE;
	}

	g_free (dec);
	return FALSE;
}

static size_t
xmms_curl_cwrite (void *ptr, size_t size, size_t nmemb, void *stream)
{
	xmms_curl_data_t *data;
	xmms_transport_t *transport = (xmms_transport_t *) stream;

	g_return_val_if_fail (transport, 0);

	data = xmms_transport_private_data_get (transport);

	XMMS_MTX_LOCK (data->mutex);
	xmms_ringbuf_wait_free (data->buffer, size*nmemb, data->mutex);
	xmms_ringbuf_write (data->buffer, ptr, size*nmemb);
	XMMS_MTX_UNLOCK (data->mutex);

	//XMMS_DBG ("Wrote %d bytes to the CURL buffer", size*nmemb);

	return size*nmemb;

}

static CURL *
xmms_curl_easy_new (xmms_transport_t *transport, const gchar *url, gint offset)
{
	struct curl_slist *headerlist = NULL;
	xmms_curl_data_t *data;
	xmms_config_value_t *val;
	CURL *curl;

	g_return_val_if_fail (transport, NULL);
	g_return_val_if_fail (url, NULL);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, NULL);

	headerlist = curl_slist_append (headerlist, "Icy-MetaData: 1");
	
	XMMS_DBG ("Setting up CURL");

	curl = curl_easy_init ();

	curl_easy_setopt (curl, CURLOPT_URL, xmms_util_decode_path (url));
	curl_easy_setopt (curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt (curl, CURLOPT_HEADER, 1);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, xmms_curl_cwrite);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, transport);
	curl_easy_setopt (curl, CURLOPT_HTTPGET, 1);
	curl_easy_setopt (curl, CURLOPT_USERAGENT, "XMMS/" XMMS_VERSION);
	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "shoutcastinfo");
	if (xmms_config_value_int_get (val) == 1)
		curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headerlist);

	if (offset)
		curl_easy_setopt (curl, CURLOPT_RESUME_FROM, offset);

	return curl;
}

static void
xmms_curl_thread (xmms_transport_t *transport)
{
	xmms_curl_data_t *data;
	struct timeval timeout;
	
	data = xmms_transport_private_data_get (transport);
	
	data->curl = xmms_curl_easy_new (transport, data->url, 0);

	curl_multi_add_handle (data->curlm, data->curl);

	while (curl_multi_perform (data->curlm, &data->running) == 
			CURLM_CALL_MULTI_PERFORM);

	XMMS_DBG ("Runned");

	FD_ZERO (&data->fdread);
	FD_ZERO (&data->fdwrite);
	FD_ZERO (&data->fdexcep);
	
	curl_multi_fdset (data->curlm, &data->fdread, 
			  &data->fdwrite, &data->fdexcep, &data->maxfd);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	while (data->run && data->running) {
		gint ret;

		ret = select (data->maxfd+1, &data->fdread, 
			      &data->fdwrite, &data->fdexcep, &timeout);

		switch (ret) {
			case -1:
				/* error ? reconnect */
				xmms_log_error ("Disconnected????");
				continue;
			case 0:
				continue;
			default:
				curl_multi_perform (data->curlm, &data->running);
				break;
		}
	}

	
}

static gboolean
xmms_curl_init (xmms_transport_t *transport, const gchar *url)
{
	xmms_curl_data_t *data;
	xmms_config_value_t *val;
	gint size;
	
	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	data = g_new0 (xmms_curl_data_t, 1);

	data->curlm = curl_multi_init ();
	data->mime = NULL;
	data->stream = FALSE;
	data->url = g_strdup (url);
	
	xmms_transport_private_data_set (transport, data);
	
	g_return_val_if_fail (data->curlm, FALSE);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "buffersize");

	size = xmms_config_value_int_get (val);

	//xmms_transport_ringbuf_resize (transport, size);
	xmms_transport_buffering_start (transport);

	data->buffer = xmms_ringbuf_new (size);
	data->mutex = g_mutex_new ();
	data->run = TRUE;

	
	/*xmms_transport_mime_type_set (transport, "audio/mpeg");*/

	
	return TRUE;
}

static void
xmms_curl_close (xmms_transport_t *transport)
{
	xmms_curl_data_t *data;

	g_return_if_fail (transport);
	
	data = xmms_transport_private_data_get (transport);
	g_return_if_fail (data);

	data->run = FALSE;

	XMMS_MTX_LOCK (data->mutex);
	xmms_ringbuf_clear (data->buffer);
	XMMS_MTX_UNLOCK (data->mutex);

	XMMS_DBG ("Waiting for thread...");
	g_thread_join (data->thread);
	XMMS_DBG ("Thread is joined");
	
	curl_multi_cleanup (data->curlm);
	curl_easy_cleanup (data->curl);

	g_mutex_free (data->mutex);
	xmms_ringbuf_destroy (data->buffer);

	if (data->genre)
		g_free (data->genre);
	if (data->name)
		g_free (data->name);
	if (data->br)
		g_free (data->br);

	g_free (data->url);

	g_free (data);
}

static gint
xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	xmms_curl_data_t *data;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);

	data = xmms_transport_private_data_get (transport);

	g_return_val_if_fail (data, -1);

	if (!data->thread) {
		xmms_config_value_t *val;
		gint size;

		data->thread = g_thread_create ((GThreadFunc) xmms_curl_thread, (gpointer) transport, TRUE, NULL);
		/* Prebuffer here ! */
		val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "prebuffersize");
		size = xmms_config_value_int_get (val);
		XMMS_DBG ("Prebuffer %d bytes...", size);
		XMMS_MTX_LOCK (data->mutex);
		xmms_ringbuf_wait_used (data->buffer, size, data->mutex);
		XMMS_MTX_UNLOCK (data->mutex);
		XMMS_DBG ("Prebuffer done ... ");

		return 0;
	}

	XMMS_MTX_LOCK (data->mutex);
	ret = xmms_ringbuf_read (data->buffer, buffer, len);
	XMMS_MTX_UNLOCK (data->mutex);

	if (xmms_ringbuf_bytes_used (data->buffer) < 8128)
		XMMS_DBG ("Buffer is very small!");

	if (!data->mime) {
		xmms_playlist_entry_t *entry;
		curl_easy_getinfo (data->curl, CURLINFO_CONTENT_TYPE, &data->mime);

		if (!data->mime && g_strncasecmp (buffer, "ICY 200 OK", 10) == 0) {
			gchar **tmp;
			gchar *header;
			gint i=0;

			data->mime = "audio/mpeg";
			data->stream = TRUE;

			header = buffer;

			buffer = strstr (header, "\r\n\r\n");

			if (!buffer) {
				xmms_log_error ("Malformated icy response");
				return -1;
			}

			*buffer = '\0';
			buffer += 2;

			len -= strlen (header) + 2;
			
			XMMS_DBG ("%s", header);
			XMMS_DBG ("First is %x", *buffer);
			
			tmp = g_strsplit (header, "\r\n", 0);
			while (tmp[i]) {
				if (g_strncasecmp (tmp[i], "icy-name:", 9) == 0)
					data->name = g_strdup (tmp[i]+9);
				if (g_strncasecmp (tmp[i], "icy-genre:", 10) == 0)
					data->genre = g_strdup (tmp[i]+10);
				if (g_strncasecmp (tmp[i], "icy-br:", 7) == 0)
					data->br = g_strdup (tmp[i]+7);
				if (g_strncasecmp (tmp[i], "icy-metaint:", 12) == 0)
					data->metaint = atoi (tmp[i]+12);

				i++;
			}

		} else if (g_strncasecmp (buffer, "HTTP/1.0 200 OK", 15) == 0) {
			gchar **tmp;
			gchar *header;
			gint i=0;

			header = buffer;

			buffer = strstr (header, "\r\n\r\n");

			if (!buffer) {
				xmms_log_error ("Malformated HTTP response");
				return -1;
			}

			*buffer = '\0';
			buffer += 2;

			len -= strlen (header) + 2;
			
			XMMS_DBG ("%s", header);
			XMMS_DBG ("First is %x", *buffer);

			tmp = g_strsplit (header, "\r\n", 0);
			while (tmp[i]) {
				if (g_strncasecmp (tmp[i], "ice-name:", 9) == 0)
					data->name = g_strdup (tmp[i]+9);
				if (g_strncasecmp (tmp[i], "ice-genre:", 10) == 0)
					data->genre = g_strdup (tmp[i]+10);
				/* bitrate can be a string here... strange ... */
				if (g_strncasecmp (tmp[i], "ice-bitrate:", 7) == 0)
					data->br = g_strdup (tmp[i]+7);

				i++;
			}

			if (data->name) {
				data->stream = TRUE;
			}

		}
		

		entry = xmms_playlist_entry_new (NULL);

		if (data->name);
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_CHANNEL, data->name);
		if (data->genre)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE, data->genre);
		if (data->br)
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_BITRATE, data->br);

		xmms_transport_entry_mediainfo_set (transport, entry);

		xmms_transport_mimetype_set (transport, data->mime);

		xmms_object_unref (entry);
	}


	return ret;

}

/*static gint
xmms_curl_seek (xmms_transport_t *transport, guint offset, gint whence)
{
	CURL *curl;
	xmms_curl_data_t *data;

	g_return_val_if_fail (transport, 0);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, 0);

	
	if (data->stream)
		return 0;

	curl = xmms_curl_easy_new (transport, data->url, offset);

	curl_multi_remove_handle (data->curlm, data->curl);
	curl_multi_add_handle (data->curlm, curl);
	curl_easy_cleanup (data->curl);
	data->curl = curl;

	if (curl_multi_perform (data->curlm, &data->running) == 
			CURLM_CALL_MULTI_PERFORM) {
		data->again = TRUE;
	}
	
	return offset;
}
*/

static gint
xmms_curl_size (xmms_transport_t *transport)
{
	gdouble size;
	xmms_curl_data_t *data;

	g_return_val_if_fail (transport, 0);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, 0);

	if (data->stream)
		return 0;

	curl_easy_getinfo (data->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);

	return (gint)size;
}
