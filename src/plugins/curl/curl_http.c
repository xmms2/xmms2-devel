/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

#include "xmms/xmms_defs.h"
#include "xmms/xmms_transportplugin.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"

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

	gboolean know_length, know_mime, shoutcast;

	struct curl_slist *http_aliases;
	struct curl_slist *http_headers;

	gchar *buffer;
	guint bufferpos, bufferlen;

	gchar *metabuffer;
	guint metabufferpos, metabufferleft;

	xmms_error_t status;
} xmms_curl_data_t;

typedef void (*handler_func_t) (xmms_transport_t *transport, xmms_medialib_session_t *session, gchar *header);

static void header_handler_contenttype (xmms_transport_t *transport,xmms_medialib_session_t *session, gchar *header);
static void header_handler_contentlength (xmms_transport_t *transport, xmms_medialib_session_t *session,gchar *header);
static void header_handler_icy_metaint (xmms_transport_t *transport, xmms_medialib_session_t *session,gchar *header);
static void header_handler_icy_name (xmms_transport_t *transport, xmms_medialib_session_t *session,gchar *header);
static void header_handler_icy_genre (xmms_transport_t *transport, xmms_medialib_session_t *session,gchar *header);
static void header_handler_icy_ok (xmms_transport_t *transport, xmms_medialib_session_t *session, gchar *header);
static void header_handler_last (xmms_transport_t *transport,xmms_medialib_session_t *session, gchar *header);
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
static guint64 xmms_curl_size (xmms_transport_t *transport);
static size_t xmms_curl_callback_write (void *ptr, size_t size, size_t nmemb, void *stream);
static size_t xmms_curl_callback_header (void *ptr, size_t size, size_t nmemb, void *stream);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, 
	                          XMMS_TRANSPORT_PLUGIN_API_VERSION,
	                          "curl_http",
	                          "Curl Transport for HTTP",
	                          XMMS_VERSION,
	                          "HTTP transport using CURL");
	
	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org");
	xmms_plugin_info_add (plugin, "INFO", "http://curl.haxx.se/libcurl");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_curl_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_curl_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_curl_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_curl_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_curl_close);

	xmms_plugin_config_property_register (plugin, "shoutcastinfo", "1",
	                                   NULL, NULL);
	xmms_plugin_config_property_register (plugin, "buffersize", "131072",
	                                   NULL, NULL);
	xmms_plugin_config_property_register (plugin, "verbose", "0", NULL, NULL);
	xmms_plugin_config_property_register (plugin, "connecttimeout", "15",
	                                   NULL, NULL);
	xmms_plugin_config_property_register (plugin, "useproxy", "0", NULL, NULL);
	xmms_plugin_config_property_register (plugin, "proxyaddress", "127.0.0.1:8080",
	                                   NULL, NULL);
	xmms_plugin_config_property_register (plugin, "authproxy", "0", NULL, NULL);
	xmms_plugin_config_property_register (plugin, "proxyuser", "user", NULL, NULL);
	xmms_plugin_config_property_register (plugin, "proxypass", "password",
	                                   NULL, NULL); 

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_STREAM);

	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_curl_can_handle (const gchar *url)
{
	g_return_val_if_fail (url, FALSE);

	if ((g_strncasecmp (url, "http", 4) == 0)) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
xmms_curl_init (xmms_transport_t *transport, const gchar *url)
{
	xmms_curl_data_t *data;
	xmms_config_property_t *val;
	gint bufsize, metaint, verbose, connecttimeout, useproxy, authproxy;
	const gchar *proxyaddress, *proxyuser, *proxypass;
	gchar proxyuserpass[90]; 

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	data = g_new0 (xmms_curl_data_t, 1);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "buffersize");
	bufsize = xmms_config_property_get_int (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "connecttimeout");
	connecttimeout = xmms_config_property_get_int (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "shoutcastinfo");
	metaint = xmms_config_property_get_int (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "verbose");
	verbose = xmms_config_property_get_int (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "useproxy");
	useproxy = xmms_config_property_get_int (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "authproxy");
	authproxy = xmms_config_property_get_int (val);
	
	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "proxyaddress");
	proxyaddress = xmms_config_property_get_string (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "proxyuser");
	proxyuser = xmms_config_property_get_string (val);

	val = xmms_plugin_config_lookup (xmms_transport_plugin_get (transport), "proxypass");
	proxypass = xmms_config_property_get_string (val);

	g_snprintf (proxyuserpass, sizeof (proxyuserpass), "%s:%s", proxyuser, proxypass);

	data->buffer = g_malloc (CURL_MAX_WRITE_SIZE);
	data->metabuffer = g_malloc (256 * 16);

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

	if (useproxy == 1) {
		curl_easy_setopt (data->curl_easy, CURLOPT_PROXY, proxyaddress);
		if (authproxy == 1) {
			curl_easy_setopt (data->curl_easy, CURLOPT_PROXYUSERPWD,
		                          proxyuserpass);
		}
	}

	curl_easy_setopt (data->curl_easy, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_VERBOSE, verbose);
	curl_easy_setopt (data->curl_easy, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt (data->curl_easy, CURLOPT_SSL_VERIFYHOST, 0);

	if (metaint == 1)
		curl_easy_setopt (data->curl_easy, CURLOPT_HTTPHEADER, data->http_headers);

	data->curl_multi = curl_multi_init ();

	curl_multi_add_handle (data->curl_multi, data->curl_easy);

	xmms_transport_private_data_set (transport, data);

	xmms_transport_buffering_start (transport);

	return TRUE;
}

static gint
xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error)
{
	xmms_curl_data_t *data;
	xmms_medialib_entry_t entry;
	gint code, handles;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	code = CURLM_CALL_MULTI_PERFORM;

	while (TRUE) {

		/* if we have data available, just pick it up */
		if (data->bufferlen) {
			len = MIN (len, data->bufferlen);
			memcpy (buffer, data->buffer, len);
			data->bufferlen -= len;
			if (data->bufferlen)
				memmove (data->buffer, data->buffer + len, data->bufferlen);
			return len;
		}

		if (code == CURLM_OK) {
			fd_set fdread, fdwrite, fdexcp;
			struct timeval timeout;
			gint ret, maxfd;

			timeout.tv_sec = 10;
			timeout.tv_usec = 0;
			
			FD_ZERO (&fdread);
			FD_ZERO (&fdwrite);
			FD_ZERO (&fdexcp);
			
			curl_multi_fdset (data->curl_multi, &fdread, &fdwrite, &fdexcp,  &maxfd);
			
			ret = select (maxfd + 1, &fdread, &fdwrite, &fdexcp, &timeout);

			if (ret == -1) {
				xmms_error_set (error, XMMS_ERROR_GENERIC, "Error select");
				break;
			} else if (ret == 0) {
				xmms_error_set (error, XMMS_ERROR_GENERIC, "Timeout");
				break;
			}

		}

		code = curl_multi_perform (data->curl_multi, &handles);

		if (code != CURLM_CALL_MULTI_PERFORM && code != CURLM_OK) {
			xmms_error_set (error, XMMS_ERROR_GENERIC, curl_multi_strerror (code));
			break;
		}

		/* done */
		if (handles == 0) {
			if (!data->know_mime) {
				xmms_medialib_session_t *session = xmms_medialib_begin_write ();
				entry = xmms_transport_medialib_entry_get (transport);
				xmms_medialib_entry_property_set_str (session, entry,
						XMMS_MEDIALIB_ENTRY_PROPERTY_MIME, NULL);
				xmms_medialib_end (session);
				xmms_medialib_entry_send_update (entry);
			}

			return 0;
		}

	}

	if (!data->know_mime) {
		xmms_medialib_session_t *session = xmms_medialib_begin_write ();
		entry = xmms_transport_medialib_entry_get (transport);
		xmms_medialib_entry_property_set_str (session, entry,
				XMMS_MEDIALIB_ENTRY_PROPERTY_MIME, NULL);
		xmms_medialib_end (session);
		xmms_medialib_entry_send_update (entry);
	}

	return -1;
}

static guint64
xmms_curl_size (xmms_transport_t *transport)
{
	xmms_curl_data_t *data;
	gint len;

	g_return_val_if_fail (transport, -1);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	if (data->know_length)
		len = data->length;
	else
		len = -1;

	return len;
}

static void
xmms_curl_close (xmms_transport_t *transport)
{
	xmms_curl_data_t *data;

	g_return_if_fail (transport);

	data = xmms_transport_private_data_get (transport);
	g_return_if_fail (data);

	curl_multi_cleanup (data->curl_multi);
	curl_easy_cleanup (data->curl_easy);

	curl_slist_free_all (data->http_aliases);
	curl_slist_free_all (data->http_headers);

	g_free (data->buffer);
	g_free (data->metabuffer);

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
	gint len;

	g_return_val_if_fail (transport, 0);

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, 0);

	g_return_val_if_fail (data->bufferlen == 0, 0);

	len = size * nmemb;

	g_return_val_if_fail (len <= CURL_MAX_WRITE_SIZE, 0);

	while (len) {
		if (data->metabufferleft) {
			gint tlen = MIN (len, data->metabufferleft);
			memcpy (data->metabuffer + data->metabufferpos, ptr, tlen);
			data->metabufferleft -= tlen;
			data->metabufferpos += tlen;
			if (!data->metabufferleft) {
				handle_shoutcast_metadata (transport, data->metabuffer);
				data->bytes_since_meta = 0;

			}
			len -= tlen;
			ptr += tlen;
		} else if (data->meta_offset && data->bytes_since_meta == data->meta_offset) {
			data->metabufferleft = (*((guchar *)ptr)) * 16;
			data->metabufferpos = 0;
			len--;
			ptr++;
			if (data->metabufferleft == 0)
				data->bytes_since_meta = 0;
		} else {
			gint tlen = len;
			if (data->meta_offset)
				tlen = MIN (tlen, data->meta_offset - data->bytes_since_meta);
			memcpy (data->buffer + data->bufferlen, ptr, tlen);
			len -= tlen;
			ptr += tlen;
			data->bytes_since_meta += tlen;
			data->bufferlen += tlen;
		}
	}

	return size * nmemb;
}

static size_t
xmms_curl_callback_header (void *ptr, size_t size, size_t nmemb, void *stream)
{
	xmms_medialib_session_t *session;
	xmms_transport_t *transport = (xmms_transport_t *) stream;
	handler_func_t func;
	gchar *header;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (ptr, -1);

	header = g_strndup ((gchar*)ptr, size * nmemb);

	func = header_handler_find (header);
	if (func != NULL) {
		gchar *val = strchr (header, ':');
		if (val) {
			g_strstrip (++val);
		} else {
			val = header;
		}
		session = xmms_medialib_begin_write ();
		func (transport, session, val);
		xmms_medialib_end (session);
	}

	g_free (header);
	return size * nmemb;
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
header_handler_contenttype (xmms_transport_t *transport, 
							xmms_medialib_session_t *session, 
							gchar *header)
{
	xmms_curl_data_t *data;
	xmms_medialib_entry_t entry;

	data = xmms_transport_private_data_get (transport);

	data->know_mime = TRUE;

	entry = xmms_transport_medialib_entry_get (transport);
	xmms_medialib_entry_property_set_str (session, entry,
			XMMS_MEDIALIB_ENTRY_PROPERTY_MIME, header);
	xmms_medialib_entry_send_update (entry);
}

static void
header_handler_contentlength (xmms_transport_t *transport, 
							  xmms_medialib_session_t *session,
							  gchar *header)
{
	xmms_curl_data_t *data;

	data = xmms_transport_private_data_get (transport);

	data->length = strtoul (header, NULL, 10);
}

static void
header_handler_icy_metaint (xmms_transport_t *transport,
							xmms_medialib_session_t *session,
							gchar *header)
{
	xmms_curl_data_t *data;

	data = xmms_transport_private_data_get (transport);

	data->meta_offset = strtoul (header, NULL, 10);
}

static void
header_handler_icy_name (xmms_transport_t *transport,
						 xmms_medialib_session_t *session,
						 gchar *header)
{
	xmms_medialib_entry_t entry;

	entry = xmms_transport_medialib_entry_get (transport);
	xmms_medialib_entry_property_set_str (session, entry, 
										  XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNEL, 
										  header);
	xmms_medialib_entry_send_update (entry);
}

static void
header_handler_icy_ok (xmms_transport_t *transport, 
					   xmms_medialib_session_t *session, 
					   gchar *header)
{
	xmms_curl_data_t *data;

	data = xmms_transport_private_data_get (transport);

	data->shoutcast = TRUE;
}

static void
header_handler_icy_genre (xmms_transport_t *transport, 
						  xmms_medialib_session_t *session,
						  gchar *header)
{
	xmms_medialib_entry_t entry;

	entry = xmms_transport_medialib_entry_get (transport);
	xmms_medialib_entry_property_set_str (session, entry, 
										  XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE, 
										  header);
	xmms_medialib_entry_send_update (entry);
}

static void
header_handler_last (xmms_transport_t *transport, 
					 xmms_medialib_session_t *session, 
					 gchar *header)
{
	xmms_curl_data_t *data;
	xmms_medialib_entry_t entry;
	gchar *mime = NULL;

	data = xmms_transport_private_data_get (transport);

	if (data->shoutcast)
		mime = "audio/mpeg";

	if (!data->know_mime) {
		entry = xmms_transport_medialib_entry_get (transport);
		xmms_medialib_entry_property_set_str (session, entry,
			XMMS_MEDIALIB_ENTRY_PROPERTY_MIME, mime);
		xmms_medialib_entry_send_update (entry);
	}

	data->know_mime = TRUE;
}

static void
handle_shoutcast_metadata (xmms_transport_t *transport, gchar *metadata)
{
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;
	xmms_curl_data_t *data;
	gchar **tags;
	guint i = 0;

	data = xmms_transport_private_data_get (transport);
	entry = xmms_transport_medialib_entry_get (transport);

	session = xmms_medialib_begin_write ();

	tags = g_strsplit (metadata, ";", 0);
	while (tags[i] != NULL) {
		if (g_strncasecmp (tags[i], "StreamTitle=", 12) == 0) {
			gchar *raw;

			raw = tags[i] + 13;
			raw[strlen (raw) - 1] = '\0';

			xmms_medialib_entry_property_set_str (session, entry, 
												  XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE, 
												  raw);
			xmms_medialib_entry_send_update (entry);
		}

		i++;
	}

	xmms_medialib_end (session);
	g_strfreev (tags);
}
