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




#include "xmms/xmms.h"
#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/magic.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

extern int errno;

/*
 * Type definitions
 */

typedef struct {
	gint fd;
	gchar *urlptr;
	const gchar *mime;
} xmms_file_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_file_can_handle (const gchar *url);
static gboolean xmms_file_init (xmms_transport_t *transport, const gchar *url);
static void xmms_file_close (xmms_transport_t *transport);
static gint xmms_file_read (xmms_transport_t *transport, gchar *buffer, guint len);
static gint xmms_file_size (xmms_transport_t *transport);
static gint xmms_file_seek (xmms_transport_t *transport, gint offset, gint whence);
static guint xmms_file_lmod (xmms_transport_t *transport);
static GList *xmms_file_list (const gchar *path);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "file",
			"File transport " XMMS_VERSION,
		 	"Plain file transport");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_file_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, xmms_file_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, xmms_file_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, xmms_file_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, xmms_file_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, xmms_file_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_LIST, xmms_file_list);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_LMOD, xmms_file_lmod);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SEEK);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_LOCAL);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_LIST);
	
	return plugin;
}

/*
 * Member functions
 */

static GList *
xmms_file_list (const gchar *path)
{
	GList *ret = NULL;
	const gchar *name;
	GDir *d;
	gchar *nurl, *urlptr;

	nurl = xmms_util_decode_path (path);

	if (g_strncasecmp (nurl, "file:", 5) == 0)
		urlptr = strchr (nurl, '/');
	else
		urlptr = nurl;

	if (!urlptr)
		return FALSE;

	if (!g_file_test (urlptr, G_FILE_TEST_IS_DIR))
		return NULL;

	
	if (!(d = g_dir_open (urlptr, 0, NULL))) {
		xmms_log ("Couldn't open directory %s", path);
		return NULL;
	}

	while ((name = g_dir_read_name (d))) {
		xmms_transport_entry_t *entry;
		xmms_transport_entry_type_t t = XMMS_TRANSPORT_ENTRY_FILE;
		gchar *tmp;

		tmp = g_strdup_printf ("%s/%s", urlptr, name);
		if (g_file_test (tmp, G_FILE_TEST_IS_DIR))
			t = XMMS_TRANSPORT_ENTRY_DIR;

		g_free (tmp);

		tmp = g_strdup_printf ("file:%s/%s", urlptr, name);
		entry = xmms_transport_entry_new (tmp, t);
		g_free (tmp);

		ret = g_list_append (ret, (gpointer) entry);
	}

	g_free (nurl);
	g_dir_close (d);

	return ret;
}

static gboolean
xmms_file_can_handle (const gchar *url)
{
	gchar *dec;
	g_return_val_if_fail (url, FALSE);

	dec = xmms_util_decode_path (url);

	XMMS_DBG ("xmms_file_can_handle (%s)", dec);
	
	if ((g_strncasecmp (dec, "file:", 5) == 0) || (dec[0] == '/')) {
		g_free (dec);
		return TRUE;
	}

	g_free (dec);
	return FALSE;
}

static gboolean
xmms_file_init (xmms_transport_t *transport, const gchar *url)
{
	gint fd;
	xmms_file_data_t *data;
	const gchar *urlptr;
	gchar *nurl;
	struct stat st;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	nurl = xmms_util_decode_path (url);
	
	XMMS_DBG ("xmms_file_init (%p, %s)", transport, nurl);

	if (g_strncasecmp (nurl, "file:", 5) == 0)
		urlptr = strchr (nurl, '/');
	else
		urlptr = nurl;

	if (!urlptr) {
		g_free (nurl);
		return FALSE;
	}

	if (stat (urlptr, &st) == -1)
		return FALSE;

	if (!S_ISREG (st.st_mode))
		return FALSE;

	XMMS_DBG ("Opening %s", urlptr);
	fd = open (urlptr, O_RDONLY | O_NONBLOCK);
	XMMS_DBG ("fd: %d", fd);
	if (fd == -1) {
		g_free (nurl);
		return FALSE;
	}

	data = g_new0 (xmms_file_data_t, 1);
	data->fd = fd;
	data->mime = NULL;
	data->urlptr = g_strdup (urlptr);
	xmms_transport_private_data_set (transport, data);

	g_free (nurl);

	data->mime = xmms_magic_mime_from_file ((const gchar*)data->urlptr);
	xmms_transport_mimetype_set (transport, (const gchar*)data->mime);
	
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
xmms_file_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	xmms_file_data_t *data;
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	if (data->mime) {
		data->mime = xmms_magic_mime_from_file ((const gchar*)data->urlptr);
		xmms_transport_mimetype_set (transport, (const gchar*)data->mime);
		data->mime = NULL;
	}

	do {
		ret = read (data->fd, buffer, len);
		if (ret == -1) {
			xmms_log_error ("errno (%d) %s", errno, strerror (errno));
		}
	} while (ret == -1 && (errno == EINTR ||
			       errno == EIO));

	if (ret == 0)
		return -1;

	return ret;
}

static gint
xmms_file_seek (xmms_transport_t *transport, gint offset, gint whence)
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

static gint
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

