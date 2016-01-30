/** @file wma.c
 *  Transport plugin for MMS streams
 *
 *  Copyright (C) 2006-2016 XMMS2 Team
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

#include <libmms/mmsx.h>
#include <errno.h>


/*
 * Type definitions
 */
typedef struct {
	mmsx_t *handle;
} xmms_mms_data_t;


/*
 * Function prototypes
 */
static gboolean xmms_mms_plugin_setup (xmms_xform_plugin_t *plugin);
static gboolean xmms_mms_init (xmms_xform_t *xform);
static void xmms_mms_destroy (xmms_xform_t *xform);
static gint xmms_mms_read (xmms_xform_t *xform, void *buffer, gint len,
                           xmms_error_t *error);


/*
 * Plugin header
 */
XMMS_XFORM_PLUGIN_DEFINE ("mms", "MMS xform plugin", XMMS_VERSION,
                          "Microsoft Media Services xform", xmms_mms_plugin_setup);

static gboolean
xmms_mms_plugin_setup (xmms_xform_plugin_t *plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_mms_init;
	methods.destroy = xmms_mms_destroy;
	methods.read = xmms_mms_read;

	xmms_xform_plugin_methods_set (plugin, &methods);

	xmms_xform_plugin_indata_add (plugin, XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url", XMMS_STREAM_TYPE_URL,
	                              "mms://*", XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_indata_add (plugin, XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url", XMMS_STREAM_TYPE_URL,
	                              "mmsh://*", XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_indata_add (plugin, XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url", XMMS_STREAM_TYPE_URL,
	                              "mmst://*", XMMS_STREAM_TYPE_END);

	xmms_xform_plugin_indata_add (plugin, XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/x-url", XMMS_STREAM_TYPE_URL,
	                              "mmsu://*", XMMS_STREAM_TYPE_END);

	return TRUE;
}


/*
 * Member functions
 */
static gboolean
xmms_mms_init (xmms_xform_t *xform)
{
	xmms_mms_data_t *data;
	const gchar *url;

	g_return_val_if_fail (xform, FALSE);

	url = xmms_xform_indata_get_str (xform, XMMS_STREAM_TYPE_URL);
	g_return_val_if_fail (url, FALSE);

	data = g_new0 (xmms_mms_data_t, 1);

	/* Last param is bandwidth. Should be configurable. */
	data->handle = mmsx_connect (NULL, NULL, url, 128 * 1024);

	if (data->handle) {
		xmms_xform_outdata_type_add (xform, XMMS_STREAM_TYPE_MIMETYPE,
		                             "application/octet-stream",
		                             XMMS_STREAM_TYPE_END);

		xmms_xform_private_data_set (xform, data);
	}

	return (data->handle) ? TRUE : FALSE;
}


static void
xmms_mms_destroy (xmms_xform_t *xform)
{
	xmms_mms_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->handle) {
		mmsx_close (data->handle);
	}

	g_free (data);
}


static gint
xmms_mms_read (xmms_xform_t *xform, void *buffer, gint len,
               xmms_error_t *error)
{
	xmms_mms_data_t *data;
	gint ret;

	g_return_val_if_fail (xform, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, -1);

	ret = mmsx_read (NULL, data->handle, buffer, len);
	if (ret < 0) {
		xmms_log_error ("errno(%d) %s", errno, strerror (errno));
		xmms_error_set (error, XMMS_ERROR_GENERIC, strerror (errno));
	}

	return ret;
}
