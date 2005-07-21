/** @file samba.c 
 *  SMB/CIFS transport plugin
 *  
 *  Copyright (C) 2004  Daniel Svensson, <nano@nittionio.nu>
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
 *
 *  Valid url:
 *  smb://user:password@host/share/path/file
 *  
 *  - host can be IP, hostname or netbios name
 *  - username and password is optional
 */

#include "xmms/xmms_defs.h"
#include "xmms/xmms_transportplugin.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_magic.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "libsmbclient.h"

extern int errno;

/*
 * Type definitions
 */

typedef struct {
	gint fd;
	gchar *urlptr;
	const gchar *mime;
} xmms_samba_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_samba_can_handle (const gchar *url);
static gboolean xmms_samba_init (xmms_transport_t *transport, 
				 const gchar *url);
static void xmms_samba_close (xmms_transport_t *transport);
static gint xmms_samba_read (xmms_transport_t *transport, 
			     gchar *buffer, guint len, xmms_error_t *error);
static gint xmms_samba_size (xmms_transport_t *transport);
static gint xmms_samba_seek (xmms_transport_t *transport, 
			     gint offset, gint whence);
static guint xmms_samba_lmod (xmms_transport_t *transport);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, 
				  XMMS_TRANSPORT_PLUGIN_API_VERSION,
				  "smb",
				  "SMB/CIFS transport " XMMS_VERSION,
				  "Access SMB/CIFS fileshares over a network.");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.se/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	xmms_plugin_info_add (plugin, "E-Mail", "nano@nittionino.nu");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, 
				xmms_samba_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, 
				xmms_samba_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, 
				xmms_samba_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, 
				xmms_samba_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, 
				xmms_samba_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, 
				xmms_samba_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_LMOD, 
				xmms_samba_lmod);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SEEK);

	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_samba_can_handle (const gchar *url)
{
	g_return_val_if_fail (url, FALSE);

	if (g_strncasecmp (url, "smb://", 6) == 0) {
		return TRUE;
	}

	return FALSE;
}


static void 
xmms_samba_auth_fn(const char *server,
		   const char *share,
		   char *workgroup, int wgmaxlen, 
		   char *username, int unmaxlen,
		   char *password, int pwmaxlen)
{
	return;
}


static gboolean
xmms_samba_init (xmms_transport_t *transport, const gchar *url)
{
	gint fd, err;
	xmms_samba_data_t *data;
	struct stat st;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	XMMS_DBG ("xmms_samba_init (%p, %s)", transport, url);

	if (!url) {
		return FALSE;
	}

	err = smbc_init (xmms_samba_auth_fn, 0);
	if (err < 0) {
		xmms_log_error ("errno (%d) %s", errno, strerror (errno));
		return FALSE;
	}

	err = smbc_stat (url, &st);
	if (err < 0) {
		xmms_log_error ("errno (%d) %s", errno, strerror (errno));
		return FALSE;
	}

	if (!S_ISREG (st.st_mode)) {
		xmms_log_error ("%s is not a regular file.", url);
		return FALSE;
	}

	fd = smbc_open (url, O_RDONLY | O_NONBLOCK, 0);
	if (fd == -1) {
		return FALSE;
	}

	data = g_new0 (xmms_samba_data_t, 1);
	data->fd = fd;
	data->mime = NULL;
	data->urlptr = g_strdup (url);
	xmms_transport_private_data_set (transport, data);

	data->mime = xmms_magic_mime_from_file ((const gchar*)data->urlptr);
	if (!data->mime) {
		g_free (data->urlptr);
		g_free (data);
		return FALSE;
	}
	xmms_transport_mimetype_set (transport, (const gchar*)data->mime);

	return TRUE;
}


static void
xmms_samba_close (xmms_transport_t *transport)
{
	gint err;
	xmms_samba_data_t *data;
	g_return_if_fail (transport);

	data = xmms_transport_private_data_get (transport);
	if (!data)
		return;

	if (data->fd != -1) {
		err = smbc_close (data->fd);
		if (err < 0) {
			xmms_log_error ("errno (%d) %s", errno, strerror (errno));
		}
	}

	g_free (data->urlptr);
	g_free (data);
}


static gint
xmms_samba_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error)
{
	xmms_samba_data_t *data;
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

	ret = smbc_read (data->fd, buffer, len); 
	if (ret < 0) {
		xmms_log_error ("errno (%d) %s", errno, strerror (errno));
		return -1;
	}

	return ret;
}


static gint
xmms_samba_seek (xmms_transport_t *transport, gint offset, gint whence)
{
	gint w = 0;
	xmms_samba_data_t *data;

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

	return smbc_lseek (data->fd, offset, w);
}


static guint
xmms_samba_lmod (xmms_transport_t *transport)
{
	gint err;
	struct stat st;
	xmms_samba_data_t *data;

	g_return_val_if_fail (transport, 0);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, 0);

	err = smbc_fstat (data->fd, &st);
	if (err < 0) {
		xmms_log_error ("errno (%d) %s", errno, strerror (errno));
		return 0;
	}

	return st.st_mtime;
}


static gint
xmms_samba_size (xmms_transport_t *transport)
{
	gint err;
	struct stat st;
	xmms_samba_data_t *data;

	g_return_val_if_fail (transport, -1);
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	err = smbc_fstat (data->fd, &st);
	if (err < 0) {
		xmms_log_error ("errno (%d) %s", errno, strerror (errno));
		return -1;
	}

	return st.st_size;
}
