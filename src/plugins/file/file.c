/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

/*
 * Type definitions
 */

typedef struct {
	gint fd;
	gchar *urlptr;
} xmms_file_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_file_can_handle (const gchar *url);
static gboolean xmms_file_init (xmms_transport_t *transport, const gchar *url);
static void xmms_file_close (xmms_transport_t *transport);
static gint xmms_file_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error);
static guint64 xmms_file_size (xmms_transport_t *transport);
static gint xmms_file_seek (xmms_transport_t *transport, guint64 offset, gint whence);
static guint xmms_file_lmod (xmms_transport_t *transport);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, 
				  XMMS_TRANSPORT_PLUGIN_API_VERSION,
				  "file",
				  "File transport " XMMS_VERSION,
				  "Plain file transport");

	if (!plugin) {
		return NULL;
	}

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_file_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_file_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_file_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_file_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_file_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_file_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_LMOD, xmms_file_lmod);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_LOCAL);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_LIST);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_file_can_handle (const gchar *url)
{
	g_return_val_if_fail (url, FALSE);

	return !g_strncasecmp (url, "file://", 7);
}

static gboolean
xmms_file_init (xmms_transport_t *transport, const gchar *url)
{
	gint fd;
	xmms_file_data_t *data;
	const gchar *urlptr;
	struct stat st;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	XMMS_DBG ("xmms_file_init (%p, %s)", transport, url);

	/* just in case our can_handle method isn't called correctly... */
	g_assert (strlen (url) >= 7);
	urlptr = &url[7];

	if (stat (urlptr, &st) == -1) {
		return FALSE;
	}

	if (!S_ISREG (st.st_mode)) {
		return FALSE;
	}

	XMMS_DBG ("Opening %s", urlptr);
	fd = open (urlptr, O_RDONLY);
	if (fd == -1) {
		return FALSE;
	}

	data = g_new0 (xmms_file_data_t, 1);
	data->fd = fd;
	data->urlptr = g_strdup (urlptr);
	xmms_transport_private_data_set (transport, data);

	return TRUE;
}

static void
xmms_file_close (xmms_transport_t *transport)
{
	xmms_file_data_t *data;
	g_return_if_fail (transport);

	data = xmms_transport_private_data_get (transport);
	if (!data)
		return;
	
	if (data->fd != -1)
		close (data->fd);

	g_free (data->urlptr);

	g_free (data);
}

static gint
xmms_file_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error)
{
	xmms_file_data_t *data;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	ret = read (data->fd, buffer, len);

	if (ret == 0)
		xmms_error_set (error, XMMS_ERROR_EOS, "End of file reached");

	if (ret == -1) {
		xmms_log_error ("errno(%d) %s", errno, strerror (errno));
		xmms_error_set (error, XMMS_ERROR_GENERIC, strerror (errno));
	}

	return ret;
}

static gint
xmms_file_seek (xmms_transport_t *transport, guint64 offset, gint whence)
{
	xmms_file_data_t *data;
	gint w = 0;

	g_return_val_if_fail (transport, -1);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	switch (whence) {
		case XMMS_TRANSPORT_SEEK_SET:
			w = SEEK_SET;
			break;
		case XMMS_TRANSPORT_SEEK_END:
			w = SEEK_END;
			break;
		case XMMS_TRANSPORT_SEEK_CUR:
			w = SEEK_CUR;
			break;
	}

	return lseek (data->fd, offset, w);
}

static guint
xmms_file_lmod (xmms_transport_t *transport)
{
	struct stat st;
	xmms_file_data_t *data;

	g_return_val_if_fail (transport, 0);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, 0);

	if (fstat (data->fd, &st) == -1) {
		return 0;
	}

	return st.st_mtime;
}

static guint64
xmms_file_size (xmms_transport_t *transport)
{
	struct stat st;
	xmms_file_data_t *data;

	g_return_val_if_fail (transport, -1);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);
	
	if (fstat (data->fd, &st) == -1) {
		return -1;
	}

	return st.st_size;
}

