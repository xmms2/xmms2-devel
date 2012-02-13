/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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
  * This plugin will provide FTP transport.
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

	gboolean stream;
	gchar *name;
	gchar *genre;

	gchar *url;
} xmms_curl_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_curl_can_handle (const gchar *url);
static gboolean xmms_curl_init (xmms_transport_t *transport, const gchar *url);
static void xmms_curl_close (xmms_transport_t *transport);
static gint xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len);
static gint xmms_curl_size (xmms_transport_t *transport);
static gint xmms_curl_seek (xmms_transport_t *transport, guint offset, gint whence);
static GList * xmms_curl_list (const gchar *url);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT,
	                          "curl_ftp",
	                          "Curl Transport for FTP",
	                          XMMS_VERSION,
	                          "FTP transport using CURL");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://xmms2.org/");
	xmms_plugin_info_add (plugin, "INFO", "http://curl.haxx.se/libcurl/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");

	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_curl_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_curl_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_curl_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_curl_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_curl_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_curl_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_LIST, xmms_curl_list);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_LIST);

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

	XMMS_DBG ("xmms_curl_ftp_can_handle (%s)", dec);

	if ((g_ascii_strncasecmp (dec, "ftp:", 4) == 0) || (dec[0] == '/')) {
		g_free (dec);
		return TRUE;
	}

	g_free (dec);
	return FALSE;
}

static size_t
xmms_curl_cwrite (void *ptr, size_t size, size_t nmemb, void  *stream)
{
	xmms_curl_data_t *data;
	xmms_transport_t *transport = (xmms_transport_t *) stream;

	g_return_val_if_fail (transport, 0);

	data = xmms_transport_private_data_get (transport);

	data->buf = g_malloc (size*nmemb);
	data->data_in_buf = size*nmemb;
	memcpy (data->buf, ptr, size*nmemb);

	return size*nmemb;
}

static CURL *
xmms_curl_easy_new (xmms_transport_t *transport, const gchar *url, gint offset)
{
	CURL *curl;

	g_return_val_if_fail (url, NULL);
	g_return_val_if_fail (transport, NULL);

	XMMS_DBG ("Setting up CURL FTP");

	curl = curl_easy_init ();

	curl_easy_setopt (curl, CURLOPT_URL, url);
	curl_easy_setopt (curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, xmms_curl_cwrite);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, transport);
	if (offset)
		curl_easy_setopt (curl, CURLOPT_RESUME_FROM, offset);

	return curl;
}

struct xmms_curl_ftp_list {
	const gchar *path;
	GList *list;
};


static size_t
xmms_curl_list_write (void *ptr, size_t size, size_t nmemb, void  *stream)
{
	gchar **rows;
	gint i=0;
	struct xmms_curl_ftp_list *fl = stream;

	((gchar*)ptr)[size*nmemb-2]='\0';

	rows = g_strsplit ((gchar *)ptr, "\r\n", 0);

	while (rows[i]) {
		xmms_transport_entry_t *e;
		xmms_transport_entry_type_t t;
		gchar *p, *tmp, *tmp2;

		if (rows[i][0] == 'd') {
			t = XMMS_TRANSPORT_ENTRY_DIR;
		} else {
			t = XMMS_TRANSPORT_ENTRY_FILE;
		}

		p = rows[i]+55;

		tmp = g_strdup_printf ("%s/%s", fl->path, p);

		tmp2 = xmms_util_encode_path (tmp);
		g_free (tmp);

		e = xmms_transport_entry_new (tmp2, t);
		g_free (tmp2);

		fl->list = g_list_append (fl->list, e);

		i++;
	}

	g_strfreev (rows);

	return size*nmemb;
}

static GList *
xmms_curl_list (const gchar *url)
{
	gchar *nurl;
	CURL *curl;
	struct xmms_curl_ftp_list *fl;
	CURLcode res;

	g_return_val_if_fail (url, FALSE);

	nurl = xmms_util_decode_path (url);

	fl = g_new (struct xmms_curl_ftp_list, 1);
	fl->path = url;
	fl->list = NULL;

	curl = curl_easy_init ();

	curl_easy_setopt (curl, CURLOPT_URL, url);
	curl_easy_setopt (curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, xmms_curl_list_write);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, fl);

	res = curl_easy_perform (curl);

	curl_easy_cleanup (curl);

	if (res != CURLE_OK) {
		return NULL;
	}

	return fl->list;
}

static gboolean
xmms_curl_init (xmms_transport_t *transport, const gchar *url)
{
	xmms_curl_data_t *data;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	data = g_new0 (xmms_curl_data_t, 1);

	data->curlm = curl_multi_init ();
	data->stream = FALSE;
	data->url = xmms_util_decode_path (url);

	xmms_transport_private_data_set (transport, data);

	g_return_val_if_fail (data->curlm, FALSE);

	data->curl = xmms_curl_easy_new (transport, url, 0);

	curl_multi_add_handle (data->curlm, data->curl);

	if (curl_multi_perform (data->curlm, &data->running) ==
	    CURLM_CALL_MULTI_PERFORM) {
		data->again = TRUE;
	}

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

	data = xmms_transport_private_data_get (transport);
	g_return_if_fail (data);

	curl_multi_cleanup (data->curlm);
	curl_easy_cleanup (data->curl);

	g_free (data->url);

	g_free (data);
}

static gint
xmms_curl_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	xmms_curl_data_t *data;
	struct timeval timeout;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);

	data = xmms_transport_private_data_get (transport);

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

