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




#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>

/*
 * Type definitions
 */

typedef struct {
	FILE *fp;
} xmms_file_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_file_can_handle (const gchar *url);
static gboolean xmms_file_open (xmms_transport_t *transport, const gchar *url);
static gint xmms_file_read (xmms_transport_t *transport, gchar *buffer, guint len);
static gboolean xmms_file_eof (xmms_transport_t *transport);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "File transport " VERSION,
							  "Plain file transport");
	
	xmms_plugin_method_add (plugin, "can_handle", xmms_file_can_handle);
	xmms_plugin_method_add (plugin, "open", xmms_file_open);
	xmms_plugin_method_add (plugin, "read", xmms_file_read);
	xmms_plugin_method_add (plugin, "eof", xmms_file_eof);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_file_can_handle (const gchar *url)
{
	g_return_val_if_fail (url, FALSE);

	XMMS_DBG ("xmms_file_can_handle (%s)", url);
	
	if ((g_strncasecmp (url, "file:", 5) == 0) || (url[0] == '/'))
		return TRUE;
	return FALSE;
}

static gboolean
xmms_file_open (xmms_transport_t *transport, const gchar *url)
{
	FILE *fp;
	xmms_file_data_t *data;
	const gchar *urlptr;

	XMMS_DBG ("xmms_file_open (%p, %s)", transport, url);
	
	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	if (g_strncasecmp (url, "file:", 5) == 0)
		urlptr = strchr (url, '/');
	else
		urlptr = url;
	if (!urlptr)
		return FALSE;

	XMMS_DBG ("Opening %s", urlptr);
	fp = fopen (urlptr, "rb");
	XMMS_DBG ("fp: %p", fp);
	if (!fp)
		return FALSE;

	data = g_new0 (xmms_file_data_t, 1);
	data->fp = fp;
	xmms_transport_private_data_set (transport, data);

	xmms_transport_mime_type_set (transport, "audio/mpeg"); /* FIXME */
	
	return TRUE;
}

static gint
xmms_file_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	xmms_file_data_t *data;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	ret = fread (buffer, 1, len, data->fp);

	return ret;
}

static gboolean
xmms_file_eof (xmms_transport_t *transport)
{
	xmms_file_data_t *data;

	g_return_val_if_fail (transport, -1);
	data = xmms_transport_private_data_get (transport);
	
	if (feof (data->fp))
		return TRUE;
	
	return FALSE;

}
