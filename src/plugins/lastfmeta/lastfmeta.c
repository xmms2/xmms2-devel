/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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

#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_log.h"

#include <curl/curl.h>

/*
 * Type definitions
 */

typedef struct xmms_lastfmeta_data_St {
	gchar url[120];
	CURL *curl_easy;
	CURLM *curl_multi;
	gint handles;
} xmms_lastfmeta_data_t;


/*
 * Function prototypes
 */

static gboolean xmms_lastfmeta_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_lastfmeta_read (xmms_xform_t *xform, xmms_sample_t *buf,
                                 gint len, xmms_error_t *err);
static void xmms_lastfmeta_destroy (xmms_xform_t *decoder);
static gboolean xmms_lastfmeta_init (xmms_xform_t *decoder);
static size_t xmms_lastfmeta_now_playing_callback (void *ptr, size_t size,
                                                   size_t nmemb, void *stream);
static gboolean xmms_lastfm_control (xmms_xform_t *xform, const gchar *cmd);
static size_t xmms_lastfm_feed_buffer (void *ptr, size_t size,
                                       size_t nmemb, void *udata);
static void xmms_lastfm_config_changed (xmms_object_t * object,
                                        gconstpointer data, gpointer udata);
static gchar *xmms_lastfm_memstr (const gchar *haystack, gint haystack_len,
                                  const gchar *needle);



/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN ("lastfmeta", "lastfmeta", XMMS_VERSION,
                   "last.fm metadata", xmms_lastfmeta_plugin_setup);

static gboolean
xmms_lastfmeta_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_lastfmeta_init;
	methods.destroy = xmms_lastfmeta_destroy;
	methods.read = xmms_lastfmeta_read;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_config_property_register (xform_plugin, "recordtoprofile",
	                                            "0", NULL, NULL);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-sync-padded",
	                              NULL);

	xmms_magic_add ("SYNC padded", "application/x-sync-padded",
	                "0 string SYNC", NULL);

	return TRUE;
}

static gboolean
xmms_lastfmeta_init (xmms_xform_t *xform)
{
	const gchar *np_fmt = "http://ws.audioscrobbler.com/radio/np.php?session=%s&debug=0";
	xmms_lastfmeta_data_t *data;
	xmms_config_property_t *val;
	const gchar *session;
	xmms_error_t err;
	gpointer buf[4];
	gint rtp;

	data = g_new (xmms_lastfmeta_data_t, 1);
	data->handles = 0;


	/* seek over the first 4 bytes (SYNC) */
	xmms_xform_read (xform, buf, 4, &err);

	if (!xmms_xform_metadata_get_str (xform, "session", &session)) {
		xmms_log_error ("No session for lastfmeta to use!");
		g_free (data);
		return FALSE;
	}

	xmms_xform_private_data_set (xform, data);

	g_snprintf (data->url, sizeof (data->url), np_fmt, session);

	data->curl_easy = curl_easy_init ();
	curl_easy_setopt (data->curl_easy, CURLOPT_URL, data->url);
	curl_easy_setopt (data->curl_easy, CURLOPT_USERAGENT,
	                  "XMMS2/" XMMS_VERSION);
	curl_easy_setopt (data->curl_easy, CURLOPT_CONNECTTIMEOUT, 15);
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEDATA, xform);
	curl_easy_setopt (data->curl_easy, CURLOPT_WRITEFUNCTION,
	                  xmms_lastfmeta_now_playing_callback);

	data->curl_multi = curl_multi_init ();
	curl_multi_add_handle (data->curl_multi, data->curl_easy);
	curl_multi_perform (data->curl_multi, &(data->handles));


	val = xmms_xform_config_lookup (xform, "recordtoprofile");
	xmms_config_property_callback_set (val, xmms_lastfm_config_changed, xform);

	rtp = xmms_config_property_get_int (val);
	if (!rtp) {
		xmms_lastfm_control (xform, "nortp");
	}

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/octet-stream",
	                             XMMS_STREAM_TYPE_END);


	return TRUE;
}


static void
xmms_lastfmeta_destroy (xmms_xform_t *xform)
{
	xmms_lastfmeta_data_t *data;
	xmms_config_property_t *val;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	curl_multi_remove_handle (data->curl_multi, data->curl_easy);
	curl_easy_cleanup (data->curl_easy);
	curl_multi_cleanup (data->curl_multi);

	val = xmms_xform_config_lookup (xform, "recordtoprofile");
	xmms_config_property_callback_remove (val, xmms_lastfm_config_changed, xform);

	g_free (data);
}


static void
xmms_lastfm_config_changed (xmms_object_t * object, gconstpointer _data,
                            gpointer udata)
{
	xmms_config_property_t *val;
	xmms_xform_t *xform;
	const gchar *name;

	xform = (xmms_xform_t *) udata;
	val = (xmms_config_property_t *) object;
	name = xmms_config_property_get_name (val);

	if (g_ascii_strcasecmp (name, "recordtoprofile")) {
		gint rtp = xmms_config_property_get_int (val);
		xmms_lastfm_control (xform, (rtp ? "rtp" : "nortp"));
	}
}

static gint
xmms_lastfmeta_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len,
                     xmms_error_t *err)
{
	xmms_lastfmeta_data_t *data;
	gint ret;

	g_return_val_if_fail (xform, 0);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	if (data->handles > 0) {
		curl_multi_perform (data->curl_multi, &(data->handles));
	}

	ret = xmms_xform_read (xform, buf, len, err);
	if (xmms_lastfm_memstr (buf, ret, "SYNC") != NULL) {
		curl_multi_add_handle (data->curl_multi, data->curl_easy);
		curl_multi_perform (data->curl_multi, &(data->handles));
	}

	return ret;
}


static size_t
xmms_lastfmeta_now_playing_callback (void *ptr, size_t size,
                                     size_t nmemb, void *stream)
{
	xmms_xform_t *xform = (xmms_xform_t *) stream;
	gchar **split;
	gint i;

	size_t ret = size * nmemb;

	split = g_strsplit (ptr, "\n", 0);
	for (i = 0; split && split[i]; i++) {
		const gchar *metakey;

		if (g_str_has_prefix (split[i], "error=")) {
			ret = 0;
			break;
		} else if (g_str_has_prefix (split[i], "artist=")) {
			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,
			xmms_xform_metadata_set_str (xform, metakey, split[i] + 7);
		} else if (g_str_has_prefix (split[i], "track=")) {
			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE;
			xmms_xform_metadata_set_str (xform, metakey, split[i] + 6);
		} else if (g_str_has_prefix (split[i], "album=")) {
			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM;
			xmms_xform_metadata_set_str (xform, metakey, split[i] + 6);
		} else if (g_str_has_prefix (split[i], "station=")) {
			metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_CHANNEL;
			xmms_xform_metadata_set_str (xform, metakey, split[i] + 8);
		}
	}
	g_strfreev (split);

	return ret;
}


static gboolean
xmms_lastfm_control (xmms_xform_t *xform, const gchar *cmd)
{
	const gchar *url = "http://ws.audioscrobbler.com/radio/control.php";
	const gchar *request_fmt = "session=%s&command=%s";
	const gchar *session;
	xmms_lastfmeta_data_t *data;
	GString *buffer;
	gchar *tmp;
	CURL *curl;

	gboolean ret = FALSE;

	g_return_val_if_fail (xform, 0);
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	if (!xmms_xform_metadata_get_str (xform, "session", &session)) {
		xmms_log_error ("No session for lastfmeta to use!");
		return FALSE;
	}

	buffer = g_string_new (NULL);

	curl = curl_easy_init ();
	curl_easy_setopt (curl, CURLOPT_USERAGENT, "XMMS2/" XMMS_VERSION);
	curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT, 15);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, buffer);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION,
	                  xmms_lastfm_feed_buffer);
	curl_easy_setopt (curl, CURLOPT_URL, url);
	curl_easy_setopt (curl, CURLOPT_POST, 1);

	tmp = g_strdup_printf (request_fmt, session, cmd);
	curl_easy_setopt (curl, CURLOPT_POSTFIELDS, tmp);

	if (curl_easy_perform (curl) == CURLE_OK) {
		if (g_str_has_prefix (buffer->str, "response=OK")) {
			ret = TRUE;
		}
	}

	g_free (tmp);
	g_string_free (buffer, TRUE);

	curl_easy_cleanup (curl);

	return ret;
}


static size_t
xmms_lastfm_feed_buffer (void *ptr, size_t size, size_t nmemb, void *udata)
{
	GString *data = (GString *) udata;

	g_string_append_len (data, ptr, nmemb);

	return size * nmemb;
}


/**
 * This is a modified version of g_strstr_len that disregards \0 chars.
 */
static gchar *
xmms_lastfm_memstr (const gchar *haystack, gint haystack_len,
                    const gchar *needle)
{
	const gchar *p, *end;
	gint needle_len, i;

	needle_len = strlen (needle);

	if (needle_len == 0) {
		return (gchar *) haystack;
	}

	if (haystack_len < needle_len) {
		return NULL;
	}

	end = haystack + haystack_len - needle_len;
	p = haystack;

	while (p <= end) {
		for (i = 0; i < needle_len; i++)
			if (p[i] != needle[i])
				goto next;

		return (gchar *) p;
next:
		p++;
	}

	return NULL;
}
