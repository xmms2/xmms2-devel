/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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
#include "xmms/xmms_bindata.h"

#include <curl/curl.h>

/*
 * Function prototypes
 */

static gboolean xmms_lastfm_init (xmms_xform_t *xform);
static void xmms_lastfm_destroy (xmms_xform_t *decoder);
static gboolean xmms_lastfm_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gchar *xmms_lastfm_handshake (xmms_xform_t *xform, CURL *curl,
                                     GString *buffer, char *error);
static gboolean xmms_lastfm_adjust (xmms_xform_t *xform, CURL *curl,
                                    GString *buffer);
static size_t xmms_lastfm_feed_buffer (void *ptr, size_t size,
                                       size_t nmemb, void *stream);


/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN ("lastfm", "last.fm transport", XMMS_VERSION,
                   "last.fm stream transport", xmms_lastfm_plugin_setup);

static gboolean
xmms_lastfm_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_lastfm_init;
	methods.destroy = xmms_lastfm_destroy;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);


	xmms_xform_plugin_config_property_register (xform_plugin, "username",
	                                            "user", NULL, NULL);
	xmms_xform_plugin_config_property_register (xform_plugin, "password",
	                                            "password", NULL, NULL);


	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url",
	                              XMMS_STREAM_TYPE_URL,
	                              "lastfm://*",
	                              XMMS_STREAM_TYPE_END);

	return TRUE;
}

/*
 * Member functions
 */
static gboolean
xmms_lastfm_init (xmms_xform_t *xform)
{
	gboolean ret = FALSE;
	GString *buffer;
	CURL *curl;
	gchar *url;
	gchar error[CURL_ERROR_SIZE] = "";

	g_return_val_if_fail (xform, FALSE);

	buffer = g_string_new (NULL);
	curl = curl_easy_init ();

	curl_easy_setopt (curl, CURLOPT_USERAGENT, "XMMS2/" XMMS_VERSION);
	curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT, 15);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, buffer);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION,
	                  xmms_lastfm_feed_buffer);
	curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, error);


	url = xmms_lastfm_handshake (xform, curl, buffer, error);
	if (!url) {
		xmms_log_error ("Last.fm handshake failed: %s", error);
		goto cleanup;
	}

	if (!xmms_lastfm_adjust (xform, curl, buffer)) {
		xmms_log_error ("Last.fm could not tune in given channel: %s", error);
		goto cleanup;
	}

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "application/x-url",
	                             XMMS_STREAM_TYPE_URL, url,
	                             XMMS_STREAM_TYPE_END);

	ret = TRUE;

cleanup:
	if (url) {
		g_free (url);
	}
	curl_easy_cleanup (curl);
	g_string_free (buffer, TRUE);

	return ret;
}


static void
xmms_lastfm_destroy (xmms_xform_t *decoder)
{
	/* a destroy function is currently required */
}


static gchar *
xmms_lastfm_handshake (xmms_xform_t *xform, CURL *curl, GString *buffer, char *error)
{
	const gchar *handshake_fmt = "http://ws.audioscrobbler.com/radio/handshake.php"
	                             "?version=0.1"
	                             "&platform=linux"
	                             "&username=%s"
	                             "&passwordmd5=%s"
	                             "&debug=0";
	const gchar *username, *password;
	xmms_config_property_t *val;
	gchar **split;
	gchar *tmp;
	gint i;
	gchar hash[33];
	gboolean failure = FALSE;

	gchar *ret = NULL;

	g_return_val_if_fail (curl, NULL);
	g_return_val_if_fail (xform, NULL);

	val = xmms_xform_config_lookup (xform, "username");
	username = xmms_config_property_get_string (val);

	val = xmms_xform_config_lookup (xform, "password");
	password = xmms_config_property_get_string (val);
	xmms_bindata_calculate_md5 ((const guchar *) password,
	                            strlen (password), hash);

	tmp = g_strdup_printf (handshake_fmt, username, hash);
	curl_easy_setopt (curl, CURLOPT_URL, tmp);

	if (curl_easy_perform (curl) == CURLE_OK) {
		split = g_strsplit (buffer->str, "\n", 0);
		for (i = 0; split && split[i]; i++) {
			if (g_str_has_prefix (split[i], "session=")) {
				if (g_ascii_strcasecmp (split[i] + 8, "FAILED") == 0) {
					failure = TRUE;
				} else {
					xmms_xform_metadata_set_str (xform, "session", split[i] + 8);
				}
			} else if (g_str_has_prefix (split[i], "stream_url=")) {
				ret = g_strdup (split[i] + 11);
			} else if (failure && g_str_has_prefix (split[i], "msg=")) {
				g_snprintf (error, CURL_ERROR_SIZE, "%s", split[i] + 4);
			}
		}
		g_strfreev (split);
	}


	g_free (tmp);
	g_string_erase (buffer, 0, -1);

	return ret;
}


static gboolean
xmms_lastfm_adjust (xmms_xform_t *xform, CURL *curl, GString *buffer)
{
	const gchar *request_fmt = "http://ws.audioscrobbler.com/radio/adjust.php"
	                           "?session=%s"
	                           "&url=%s"
	                           "&debug=0";
	const gchar *url, *session;
	gchar *tmp;

	gboolean ret = FALSE;

	g_return_val_if_fail (curl, FALSE);
	g_return_val_if_fail (xform, FALSE);

	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);

	if (!xmms_xform_metadata_get_str (xform, "session", &session)) {
		return FALSE;
	}

	tmp = g_strdup_printf (request_fmt, session, url);
	curl_easy_setopt (curl, CURLOPT_URL, tmp);

	if (curl_easy_perform (curl) == CURLE_OK) {
		if (g_str_has_prefix (buffer->str, "response=OK")) {
			ret = TRUE;
		}
	}

	g_free (tmp);
	g_string_erase (buffer, 0, -1);

	return ret;
}


static size_t
xmms_lastfm_feed_buffer (void *ptr, size_t size, size_t nmemb, void *udata)
{
	GString *data = (GString *) udata;

	g_string_append_len (data, ptr, nmemb);

	return size * nmemb;
}
