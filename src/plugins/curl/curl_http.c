/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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

#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>
#include <xmms/xmms_medialib.h>

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

	guint meta_offset;

	gchar *url;

	struct curl_slist *http_200_aliases;
	struct curl_slist *http_req_headers;

	gchar *buffer;
	guint bufferpos, bufferlen;

	gint curl_code;

	gboolean done;

	xmms_error_t status;

	gboolean broken_version;
} xmms_curl_data_t;

typedef void (*handler_func_t) (xmms_xform_t *xform, gchar *header);

static void header_handler_contentlength (xmms_xform_t *xform, gchar *header);
static void header_handler_icy_metaint (xmms_xform_t *xform, gchar *header);
static void header_handler_icy_name (xmms_xform_t *xform, gchar *header);
static void header_handler_icy_genre (xmms_xform_t *xform, gchar *header);
static handler_func_t header_handler_find (gchar *header);

typedef struct {
	const gchar *name;
	handler_func_t func;
} handler_t;

handler_t handlers[] = {
	{ "content-length", header_handler_contentlength },
	{ "icy-metaint", header_handler_icy_metaint },
	{ "icy-name", header_handler_icy_name },
	{ "icy-genre", header_handler_icy_genre },
/*	{ "\r\n", header_handler_last }, */
	{ NULL, NULL }
};

/*
 * Function prototypes
 */

static gboolean xmms_curl_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gboolean xmms_curl_init (xmms_xform_t *xform);
static void xmms_curl_destroy (xmms_xform_t *xform);
static gint fill_buffer (xmms_xform_t *xform, xmms_curl_data_t *data, xmms_error_t *error);
static gint xmms_curl_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error);
/* static gint64 xmms_curl_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *error); */
static size_t xmms_curl_callback_write (void *ptr, size_t size, size_t nmemb, void *stream);
static size_t xmms_curl_callback_header (void *ptr, size_t size, size_t nmemb, void *stream);

static void xmms_curl_free_data (xmms_curl_data_t *data);

/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN_DEFINE ("curl",
                          "Curl Transport for HTTP",
                          XMMS_VERSION,
                          "HTTP transport using CURL",
                          xmms_curl_plugin_setup);

static gboolean
xmms_curl_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_curl_init;
	methods.destroy = xmms_curl_destroy;
	methods.read = xmms_curl_read;
	/*
	methods.seek = xmms_curl_seek;
	*/

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	/*
	  xmms_plugin_info_add (plugin, "URL", "http://xmms2.org");
	  xmms_plugin_info_add (plugin, "INFO", "http://curl.haxx.se/libcurl");
	  xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	*/

	xmms_xform_plugin_config_property_register (xform_plugin, "shoutcastinfo",
	                                            "1", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "verbose",
	                                            "0", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "connecttimeout",
	                                            "15", NULL, NULL);
	/* TODO is this timeout of 10 seconds really appropriate? */
	xmms_xform_plugin_config_property_register (xform_plugin, "readtimeout",
	                                            "10", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "useproxy",
	                                            "0", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "proxyaddress",
	                                            "127.0.0.1:8080", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "authproxy",
	                                            "0", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "proxyuser",
	                                            "user", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "proxypass",
	                                            "password", NULL, NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "http://*",
	                              XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "https://*",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

/*
 * Member functions
 */

static gboolean
xmms_curl_init (xmms_xform_t *xform)
{
	xmms_curl_data_t *data;
	xmms_config_property_t *val;
	xmms_error_t error;
	gint metaint, verbose, connecttimeout, readtimeout, useproxy, authproxy;
	const gchar *proxyaddress, *proxyuser, *proxypass;
	gchar proxyuserpass[90];
	const gchar *url;
	curl_version_info_data *version;

	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_curl_data_t, 1);
	data->broken_version = FALSE;

	val = xmms_xform_config_lookup (xform, "connecttimeout");
	connecttimeout = xmms_config_property_get_int (val);

	val = xmms_xform_config_lookup (xform, "readtimeout");
	readtimeout = xmms_config_property_get_int (val);

	val = xmms_xform_config_lookup (xform, "shoutcastinfo");
	metaint = xmms_config_property_get_int (val);

	val = xmms_xform_config_lookup (xform, "verbose");
	verbose = xmms_config_property_get_int (val);

	val = xmms_xform_config_lookup (xform, "useproxy");
	useproxy = xmms_config_property_get_int (val);

	val = xmms_xform_config_lookup (xform, "authproxy");
	authproxy = xmms_config_property_get_int (val);

	val = xmms_xform_config_lookup (xform, "proxyaddress");
	proxyaddress = xmms_config_property_get_string (val);

	val = xmms_xform_config_lookup (xform, "proxyuser");
	proxyuser = xmms_config_property_get_string (val);

	val = xmms_xform_config_lookup (xform, "proxypass");
	proxypass = xmms_config_property_get_string (val);

	g_snprintf (proxyuserpass, sizeof (proxyuserpass), "%s:%s", proxyuser,
	            proxypass);

	data->buffer = g_malloc (CURL_MAX_WRITE_SIZE);
	data->url = g_strdup (url);

	/* check for broken version of curl here */
	version = curl_version_info (CURLVERSION_NOW);
	XMMS_DBG ("Using version %s of libcurl", version->version);
	if (version->version_num == 0x071001 || version->version_num == 0x071002) {
		xmms_log_info ("**********************************************");
		xmms_log_info ("Your version of libcurl is incompatible with");
		xmms_log_info ("XMMS2 and you will not be able to stream shout/ice-cast");
		xmms_log_info ("radio stations. Please consider downgrade to 7.15 or");
		xmms_log_info ("upgrade to a more recent version than 7.16.2");
		xmms_log_info ("**********************************************");
		data->broken_version = TRUE;
	}

	if (!data->broken_version && metaint == 1) {
		data->http_req_headers = curl_slist_append (data->http_req_headers,
		                                            "Icy-MetaData: 1");
	}

	data->curl_easy = curl_easy_init ();

	curl_easy_setopt (data->curl_easy, CURLOPT_URL, data->url);
	curl_easy_setopt (data->curl_easy, CURLOPT_HEADER, 0);
	curl_easy_setopt (data->curl_easy, CURLOPT_HTTPGET, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt (data->curl_easy, CURLOPT_USERAGENT,
	                  "XMMS2/" XMMS_VERSION);
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEHEADER, xform);
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEDATA, xform);
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEFUNCTION,
	                  xmms_curl_callback_write);
	curl_easy_setopt (data->curl_easy, CURLOPT_HEADERFUNCTION,
	                  xmms_curl_callback_header);
	curl_easy_setopt (data->curl_easy, CURLOPT_CONNECTTIMEOUT, connecttimeout);
	curl_easy_setopt (data->curl_easy, CURLOPT_LOW_SPEED_TIME, readtimeout);
	curl_easy_setopt (data->curl_easy, CURLOPT_LOW_SPEED_LIMIT, 1);

	if (!data->broken_version) {
		data->http_200_aliases = curl_slist_append (data->http_200_aliases,
		                                            "ICY 200 OK");
		data->http_200_aliases = curl_slist_append (data->http_200_aliases,
		                                            "ICY 402 Service Unavailabe");
		curl_easy_setopt (data->curl_easy, CURLOPT_HTTP200ALIASES,
		                  data->http_200_aliases);
	}

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

	if (data->http_req_headers) {
		curl_easy_setopt (data->curl_easy, CURLOPT_HTTPHEADER,
		                  data->http_req_headers);
	}

	data->curl_multi = curl_multi_init ();
	data->curl_code = CURLM_CALL_MULTI_PERFORM;

	curl_multi_add_handle (data->curl_multi, data->curl_easy);

	xmms_xform_private_data_set (xform, data);

	/* perform initial fill to see if it contains shoutcast metadata or not */
	if (fill_buffer (xform, data, &error) <= 0) {
		/* something went wrong */
		xmms_xform_private_data_set (xform, NULL);
		xmms_curl_free_data (data);
		return FALSE;
	}

	if (data->meta_offset > 0) {
		XMMS_DBG ("icy-metadata detected");
		xmms_xform_auxdata_set_int (xform, "meta_offset", data->meta_offset);

		xmms_xform_outdata_type_add (xform,
		                             XMMS_STREAM_TYPE_MIMETYPE,
		                             "application/x-icy-stream",
		                             XMMS_STREAM_TYPE_END);
	} else {
		xmms_xform_outdata_type_add (xform,
		                             XMMS_STREAM_TYPE_MIMETYPE,
		                             "application/octet-stream",
		                             XMMS_STREAM_TYPE_END);
	}

	return TRUE;
}

static gint
fill_buffer (xmms_xform_t *xform, xmms_curl_data_t *data, xmms_error_t *error)
{
	gint handles;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (data, -1);
	g_return_val_if_fail (error, -1);

	while (TRUE) {
		if (data->curl_code == CURLM_OK) {
			fd_set fdread, fdwrite, fdexcp;
			struct timeval timeout;
			gint ret, maxfd;
			glong milliseconds;

			FD_ZERO (&fdread);
			FD_ZERO (&fdwrite);
			FD_ZERO (&fdexcp);

			curl_multi_fdset (data->curl_multi, &fdread, &fdwrite, &fdexcp,
			                  &maxfd);
			curl_multi_timeout (data->curl_multi, &milliseconds);

			if (milliseconds <= 0) {
				milliseconds = 1000;
			}

			timeout.tv_sec = milliseconds / 1000;
			timeout.tv_usec = (milliseconds % 1000) * 1000;

			ret = select (maxfd + 1, &fdread, &fdwrite, &fdexcp, &timeout);

			if (ret == -1) {
				xmms_error_set (error, XMMS_ERROR_GENERIC, "Error select");
				return -1;
			}
		}

		data->curl_code = curl_multi_perform (data->curl_multi, &handles);

		if (data->curl_code != CURLM_CALL_MULTI_PERFORM &&
		    data->curl_code != CURLM_OK) {

			xmms_error_set (error, XMMS_ERROR_GENERIC,
			                curl_multi_strerror (data->curl_code));
			return -1;
		}

		/* done */
		if (handles == 0) {
			CURLMsg *curlmsg;
			gint messages;

			do {
				curlmsg = curl_multi_info_read (data->curl_multi, &messages);

				if (curlmsg == NULL)
					break;

				if (curlmsg->msg == CURLMSG_DONE && curlmsg->data.result != CURLE_OK) {
					xmms_log_error ("Curl fill_buffer returned error: (%d) %s",
					                curlmsg->data.result,
					                curl_easy_strerror (curlmsg->data.result));
				} else {
					XMMS_DBG ("Curl fill_buffer returned unknown message (%d)", curlmsg->msg);
				}
			} while (messages > 0);

			data->done = TRUE;
			return 0;
		}

		if (data->bufferlen > 0) {
			return 1;
		}
	}
}

static gint
xmms_curl_read (xmms_xform_t *xform, void *buffer, gint len,
                xmms_error_t *error)
{
	xmms_curl_data_t *data;
	gint ret;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	if (data->done)
		return 0;

	while (TRUE) {

		/* if we have data available, just pick it up (even if there's
		   less bytes available than was requested) */
		if (data->bufferlen) {
			len = MIN (len, data->bufferlen);
			memcpy (buffer, data->buffer, len);
			data->bufferlen -= len;
			if (data->bufferlen) {
				memmove (data->buffer, data->buffer + len, data->bufferlen);
			}
			return len;
		}

		ret = fill_buffer (xform, data, error);

		if (ret == 0 || ret == -1) {
			return ret;
		}
	}
}

static void
xmms_curl_destroy (xmms_xform_t *xform)
{
	xmms_curl_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	xmms_curl_free_data (data);
}

/*
 * CURL callback functions
 */

static size_t
xmms_curl_callback_write (void *ptr, size_t size, size_t nmemb, void *stream)
{
	xmms_curl_data_t *data;
	xmms_xform_t *xform = (xmms_xform_t *) stream;
	gint len;

	g_return_val_if_fail (xform, 0);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	len = size * nmemb;

	g_return_val_if_fail ((data->bufferlen + len) <= CURL_MAX_WRITE_SIZE, 0);

	memcpy (data->buffer + data->bufferlen, ptr, len);
	data->bufferlen = data->bufferlen + len;

	return len;
}

static int
strlen_no_crlf (char *ptr, int len) {
	int ep = len - 1;
	for (; ep >= 0; --ep) {
		if (ptr[ep] != '\r' && ptr[ep] != '\n') {
			break;
		}
	}
	return ep + 1;
}

static size_t
xmms_curl_callback_header (void *ptr, size_t size, size_t nmemb, void *stream)
{
	xmms_xform_t *xform = (xmms_xform_t *) stream;
	handler_func_t func;
	gchar *header;

	XMMS_DBG ("%.*s", strlen_no_crlf ((char*)ptr, size * nmemb), (char*)ptr);

	g_return_val_if_fail (xform, 0);
	g_return_val_if_fail (ptr, 0);

	header = g_strndup ((gchar*)ptr, size * nmemb);

	func = header_handler_find (header);
	if (func != NULL) {
		gchar *val = strchr (header, ':');
		if (val) {
			g_strstrip (++val);
		} else {
			val = header;
		}
		func (xform, val);
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

		if (g_ascii_strncasecmp (handlers[i].name, header, len) == 0) {
			return handlers[i].func;
		}
	}

	return NULL;
}

static void
header_handler_contentlength (xmms_xform_t *xform,
                              gchar *header)
{
	int length;
	const gchar *metakey;

	length = strtoul (header, NULL, 10);

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE,
	xmms_xform_metadata_set_int (xform, metakey, length);
}

static void
header_handler_icy_metaint (xmms_xform_t *xform,
                            gchar *header)
{
	xmms_curl_data_t *data;

	data = xmms_xform_private_data_get (xform);

	data->meta_offset = strtoul (header, NULL, 10);
}

static void
header_handler_icy_name (xmms_xform_t *xform,
                         gchar *header)
{
	const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNEL;
	xmms_xform_metadata_set_str (xform, metakey, header);
}

static void
header_handler_icy_genre (xmms_xform_t *xform,
                          gchar *header)
{
	const gchar *metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE;
	xmms_xform_metadata_set_str (xform, metakey, header);
}

static void
xmms_curl_free_data (xmms_curl_data_t *data)
{
	g_return_if_fail (data);

	curl_multi_cleanup (data->curl_multi);
	curl_easy_cleanup (data->curl_easy);

	curl_slist_free_all (data->http_200_aliases);
	curl_slist_free_all (data->http_req_headers);

	g_free (data->buffer);

	g_free (data->url);
	g_free (data);
}

/*
static gint64
xmms_curl_seek (xmms_xform_t *xform, gint64 offset,
                xmms_xform_seek_mode_t whence, xmms_error_t *error) {
	xmms_error_set (error, XMMS_ERROR_INVAL, "Couldn't seek");
	return -1;
}
*/
